"""Chargement des variables d'environnement et constantes globales."""
from __future__ import annotations

import os
from dataclasses import dataclass
from dotenv import load_dotenv

load_dotenv()


def _get_int(key: str, default: int) -> int:
    raw = os.getenv(key)
    if raw is None or raw.strip() == "":
        return default
    try:
        return int(raw)
    except ValueError:
        return default


@dataclass(frozen=True)
class Settings:
    discord_token: str
    guild_id: int
    default_channel_id: int
    resellers_role_id: int | None
    poll_interval: int
    request_delay: float
    database_path: str = "vinted_bot.db"


SETTINGS = Settings(
    discord_token=os.getenv("DISCORD_TOKEN", ""),
    guild_id=_get_int("GUILD_ID", 0),
    default_channel_id=_get_int("DEFAULT_CHANNEL_ID", 0),
    resellers_role_id=_get_int("RESELLERS_ROLE_ID", 0) or None,
    poll_interval=_get_int("POLL_INTERVAL", 60),
    request_delay=float(os.getenv("REQUEST_DELAY", "4")),
)


# Marques considerees premium pour le scoring
PREMIUM_BRANDS = {
    "nike", "adidas", "levi's", "levis", "stone island", "stussy", "stüssy",
    "carhartt", "the north face", "north face", "patagonia", "arcteryx",
    "arc'teryx", "supreme", "palace", "ralph lauren", "polo ralph lauren",
    "tommy hilfiger", "lacoste", "moncler", "canada goose", "jordan",
    "yeezy", "new balance", "burberry", "gucci", "louis vuitton", "prada",
    "balenciaga", "off-white", "off white", "fear of god", "essentials",
}

# Endpoints Vinted (API non officielle, peut bouger)
VINTED_BASE_URL = "https://www.vinted.fr"
VINTED_CATALOG_ENDPOINT = "/api/v2/catalog/items"
VINTED_ITEM_ENDPOINT = "/api/v2/items"

DEFAULT_USER_AGENTS = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
    "(KHTML, like Gecko) Version/17.4 Safari/605.1.15",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/123.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:125.0) Gecko/20100101 Firefox/125.0",
]
