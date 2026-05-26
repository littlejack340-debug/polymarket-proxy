"""Calcul du score de deal et de la valeur marche estimee."""
from __future__ import annotations

import logging
import statistics
import time
from dataclasses import dataclass

from config import PREMIUM_BRANDS
from vinted_scraper import VintedItem, VintedScraper

logger = logging.getLogger(__name__)


@dataclass
class DealEvaluation:
    score: int           # 0..5
    market_price: float  # prix median observe sur Vinted pour articles similaires
    margin_eur: float
    margin_pct: float
    sample_size: int     # nb d'annonces utilisees pour estimer le marche


# Etats consideres comme "neuf" (libelles FR Vinted)
NEW_LIKE_STATUSES = {
    "neuf avec etiquette", "neuf sans etiquette", "neuf",
    "new with tags", "new without tags",
}


def _is_premium_brand(brand: str) -> bool:
    return brand.strip().lower() in PREMIUM_BRANDS if brand else False


def _is_new_like(status: str) -> bool:
    return status.strip().lower() in NEW_LIKE_STATUSES if status else False


async def estimate_market_price(
    scraper: VintedScraper, item: VintedItem, *, sample_size: int = 20
) -> tuple[float, int]:
    """Estime le prix marche en cherchant des annonces similaires sur Vinted.

    Retourne (prix_median, taille_echantillon). Si pas assez de donnees,
    retourne (0.0, 0).
    """
    if not item.brand and not item.title:
        return 0.0, 0

    # On reconstruit un filtre minimal pour requeter le scraper.
    from database import Filter  # import tardif pour eviter le cycle

    probe = Filter(
        id=0,
        name="_probe",
        brand=item.brand or None,
        category=None,
        size=item.size or None,
        max_price=None,
        keywords=[w for w in item.title.split() if len(w) > 2][:3],
    )

    try:
        similar = await scraper.search(probe, per_page=sample_size)
    except Exception as exc:  # noqa: BLE001
        logger.warning("estimate_market_price: search failed: %s", exc)
        return 0.0, 0

    prices = [
        s.price for s in similar
        if s.item_id != item.item_id and s.price > 0
    ]
    if len(prices) < 3:
        return 0.0, len(prices)

    return float(statistics.median(prices)), len(prices)


def score_deal(item: VintedItem, market_price: float) -> DealEvaluation:
    """Implementation de la matrice de score decrite dans le prompt."""
    if market_price > 0 and item.price > 0:
        margin_eur = market_price - item.price
        margin_pct = (margin_eur / market_price) * 100.0
    else:
        margin_eur = 0.0
        margin_pct = 0.0

    score = 0
    if margin_pct >= 50:
        score += 2
    elif margin_pct >= 30:
        score += 1

    if _is_new_like(item.status):
        score += 1
    if _is_premium_brand(item.brand):
        score += 1

    # Fraicheur : annonce < 10 min
    if item.created_at_ts:
        age_sec = time.time() - item.created_at_ts
        if 0 <= age_sec < 600:
            score += 1

    score = max(0, min(score, 5))

    return DealEvaluation(
        score=score,
        market_price=market_price,
        margin_eur=margin_eur,
        margin_pct=margin_pct,
        sample_size=0,  # rempli par l'appelant si dispo
    )


async def evaluate(scraper: VintedScraper, item: VintedItem) -> DealEvaluation:
    """Pipeline complet : estime le marche puis calcule le score."""
    market_price, n = await estimate_market_price(scraper, item)
    evaluation = score_deal(item, market_price)
    evaluation.sample_size = n
    return evaluation
