"""Client HTTP pour l'API non officielle de Vinted.

Vinted n'expose pas d'API publique. Cette implementation s'appuie sur les
endpoints utilises par le site web (api/v2/catalog/items, api/v2/items/<id>).
Elle gere la session/cookies, le User-Agent rotatif, le rate limiting et le
backoff exponentiel.
"""
from __future__ import annotations

import asyncio
import logging
import random
import time
from dataclasses import dataclass, field
from typing import Any

import httpx

from config import (
    DEFAULT_USER_AGENTS,
    SETTINGS,
    VINTED_BASE_URL,
    VINTED_CATALOG_ENDPOINT,
    VINTED_ITEM_ENDPOINT,
)
from database import Filter

logger = logging.getLogger(__name__)


@dataclass
class VintedItem:
    item_id: str
    title: str
    brand: str
    size: str
    status: str            # ex: "Tres bon etat", "Neuf avec etiquette"
    price: float
    currency: str
    url: str
    photo_url: str | None
    seller_login: str
    created_at_ts: int | None    # epoch seconds si dispo, sinon None
    raw: dict[str, Any] = field(default_factory=dict)


class VintedScraper:
    """Client HTTP asynchrone avec rate-limit, retries et rotation de UA."""

    def __init__(
        self,
        base_url: str = VINTED_BASE_URL,
        request_delay: float = SETTINGS.request_delay,
    ) -> None:
        self.base_url = base_url
        self.request_delay = request_delay
        self._last_request_at: float = 0.0
        self._client: httpx.AsyncClient | None = None
        self._lock = asyncio.Lock()

    async def __aenter__(self) -> "VintedScraper":
        await self.start()
        return self

    async def __aexit__(self, exc_type, exc, tb) -> None:
        await self.close()

    async def start(self) -> None:
        if self._client is not None:
            return
        self._client = httpx.AsyncClient(
            base_url=self.base_url,
            timeout=httpx.Timeout(15.0),
            follow_redirects=True,
            headers={
                "Accept": "application/json, text/plain, */*",
                "Accept-Language": "fr-FR,fr;q=0.9,en;q=0.8",
            },
        )
        # Premier GET sur la home pour recuperer les cookies de session.
        await self._bootstrap_session()

    async def close(self) -> None:
        if self._client is not None:
            await self._client.aclose()
            self._client = None

    async def _bootstrap_session(self) -> None:
        assert self._client is not None
        try:
            await self._client.get("/", headers={"User-Agent": random.choice(DEFAULT_USER_AGENTS)})
        except httpx.HTTPError as exc:
            logger.warning("Bootstrap Vinted session failed: %s", exc)

    async def _respect_rate_limit(self) -> None:
        async with self._lock:
            elapsed = time.monotonic() - self._last_request_at
            wait = self.request_delay - elapsed
            if wait > 0:
                await asyncio.sleep(wait)
            self._last_request_at = time.monotonic()

    async def _request(
        self,
        method: str,
        path: str,
        *,
        params: dict[str, Any] | None = None,
        max_retries: int = 4,
    ) -> dict[str, Any] | None:
        assert self._client is not None, "Call start() before issuing requests"

        backoff = 2.0
        for attempt in range(1, max_retries + 1):
            await self._respect_rate_limit()
            headers = {"User-Agent": random.choice(DEFAULT_USER_AGENTS)}
            try:
                resp = await self._client.request(method, path, params=params, headers=headers)
            except httpx.HTTPError as exc:
                logger.warning("HTTP error on %s (attempt %d/%d): %s", path, attempt, max_retries, exc)
                await asyncio.sleep(backoff)
                backoff *= 2
                continue

            if resp.status_code == 200:
                try:
                    return resp.json()
                except ValueError:
                    logger.warning("Non-JSON 200 response on %s", path)
                    return None

            if resp.status_code in (401, 403):
                # Session probablement expiree, on rebootstrap
                logger.info("Vinted refused (%d) — refreshing session", resp.status_code)
                await self._bootstrap_session()
            elif resp.status_code == 429:
                logger.warning("Rate limited by Vinted, backing off %.1fs", backoff)
            elif 500 <= resp.status_code < 600:
                logger.warning("Vinted server error %d", resp.status_code)
            else:
                logger.warning("Unexpected status %d on %s", resp.status_code, path)
                return None

            await asyncio.sleep(backoff)
            backoff *= 2

        logger.error("Giving up on %s after %d attempts", path, max_retries)
        return None

    # ---------- API publique ----------

    async def search(self, flt: Filter, *, per_page: int = 24) -> list[VintedItem]:
        """Cherche les annonces matching un filtre, plus recents en premier."""
        params: dict[str, Any] = {
            "page": 1,
            "per_page": per_page,
            "order": "newest_first",
        }
        # On combine marque + mots-cles dans la query texte. La granularite
        # exacte de l'API Vinted varie selon les pays.
        query_parts: list[str] = []
        if flt.brand:
            query_parts.append(flt.brand)
        query_parts.extend(flt.keywords)
        if query_parts:
            params["search_text"] = " ".join(query_parts)
        if flt.max_price is not None:
            params["price_to"] = flt.max_price
            params["currency"] = "EUR"
        if flt.size:
            params["size_ids"] = flt.size  # Vinted attend des IDs, accepte parfois libelles
        if flt.category:
            params["catalog_ids"] = flt.category

        data = await self._request("GET", VINTED_CATALOG_ENDPOINT, params=params)
        if not data:
            return []
        raw_items = data.get("items") or []
        return [self._parse_item(raw) for raw in raw_items if raw]

    async def get_item(self, item_id: str) -> VintedItem | None:
        data = await self._request("GET", f"{VINTED_ITEM_ENDPOINT}/{item_id}")
        if not data:
            return None
        raw = data.get("item")
        return self._parse_item(raw) if raw else None

    # ---------- helpers internes ----------

    def _parse_item(self, raw: dict[str, Any]) -> VintedItem:
        price_block = raw.get("price") or {}
        # L'API renvoie tantot {"amount": "12.50", "currency_code": "EUR"} tantot un float
        if isinstance(price_block, dict):
            try:
                price = float(price_block.get("amount", 0) or 0)
            except (TypeError, ValueError):
                price = 0.0
            currency = price_block.get("currency_code", "EUR")
        else:
            try:
                price = float(price_block)
            except (TypeError, ValueError):
                price = 0.0
            currency = raw.get("currency", "EUR")

        photos = raw.get("photos") or []
        photo_url = None
        if photos and isinstance(photos[0], dict):
            photo_url = photos[0].get("url") or photos[0].get("full_size_url")
        elif raw.get("photo"):
            photo_url = (raw["photo"] or {}).get("url")

        user = raw.get("user") or {}
        item_id = str(raw.get("id") or raw.get("item_id") or "")
        title = str(raw.get("title") or "")
        url = raw.get("url") or f"{self.base_url}/items/{item_id}"

        created_at_ts: int | None = None
        for key in ("created_at_ts", "photo_high_resolution_ts", "created_at"):
            val = raw.get(key)
            if isinstance(val, (int, float)):
                created_at_ts = int(val)
                break

        return VintedItem(
            item_id=item_id,
            title=title,
            brand=str(raw.get("brand_title") or raw.get("brand") or ""),
            size=str(raw.get("size_title") or raw.get("size") or ""),
            status=str(raw.get("status") or raw.get("condition") or ""),
            price=price,
            currency=currency,
            url=url,
            photo_url=photo_url,
            seller_login=str(user.get("login") or ""),
            created_at_ts=created_at_ts,
            raw=raw,
        )
