"""Couche SQLite : persistance des deals deja postes, des filtres et des stats."""
from __future__ import annotations

import json
from dataclasses import dataclass, asdict
from datetime import datetime, timezone
from typing import Any

import aiosqlite

from config import SETTINGS


@dataclass
class Filter:
    id: int
    name: str
    brand: str | None
    category: str | None
    size: str | None
    max_price: float | None
    keywords: list[str]

    def to_payload(self) -> dict[str, Any]:
        data = asdict(self)
        return data


SCHEMA = """
CREATE TABLE IF NOT EXISTS seen_items (
    item_id TEXT PRIMARY KEY,
    posted_at TEXT NOT NULL,
    score INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS filters (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    brand TEXT,
    category TEXT,
    size TEXT,
    max_price REAL,
    keywords TEXT NOT NULL DEFAULT '[]'
);

CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS stats (
    key TEXT PRIMARY KEY,
    value INTEGER NOT NULL DEFAULT 0
);
"""


class Database:
    """Wrapper async simple autour de aiosqlite."""

    def __init__(self, path: str = SETTINGS.database_path) -> None:
        self.path = path

    async def init(self) -> None:
        async with aiosqlite.connect(self.path) as db:
            await db.executescript(SCHEMA)
            await db.commit()

    # ---------- seen items (dedup) ----------

    async def is_seen(self, item_id: str) -> bool:
        async with aiosqlite.connect(self.path) as db:
            cur = await db.execute(
                "SELECT 1 FROM seen_items WHERE item_id = ?", (item_id,)
            )
            row = await cur.fetchone()
            return row is not None

    async def mark_seen(self, item_id: str, score: int) -> None:
        async with aiosqlite.connect(self.path) as db:
            await db.execute(
                "INSERT OR IGNORE INTO seen_items(item_id, posted_at, score) "
                "VALUES (?, ?, ?)",
                (item_id, datetime.now(timezone.utc).isoformat(), score),
            )
            await db.commit()

    async def prune_seen(self, keep_last: int = 5000) -> None:
        """Empeche la table seen_items de gonfler indefiniment."""
        async with aiosqlite.connect(self.path) as db:
            await db.execute(
                "DELETE FROM seen_items WHERE item_id NOT IN ("
                "SELECT item_id FROM seen_items ORDER BY posted_at DESC LIMIT ?"
                ")",
                (keep_last,),
            )
            await db.commit()

    # ---------- filters ----------

    async def add_filter(
        self,
        name: str,
        brand: str | None,
        category: str | None,
        size: str | None,
        max_price: float | None,
        keywords: list[str],
    ) -> Filter:
        async with aiosqlite.connect(self.path) as db:
            cur = await db.execute(
                "INSERT INTO filters(name, brand, category, size, max_price, keywords) "
                "VALUES (?, ?, ?, ?, ?, ?)",
                (name, brand, category, size, max_price, json.dumps(keywords)),
            )
            await db.commit()
            return Filter(
                id=cur.lastrowid or 0,
                name=name,
                brand=brand,
                category=category,
                size=size,
                max_price=max_price,
                keywords=keywords,
            )

    async def remove_filter(self, name: str) -> bool:
        async with aiosqlite.connect(self.path) as db:
            cur = await db.execute("DELETE FROM filters WHERE name = ?", (name,))
            await db.commit()
            return cur.rowcount > 0

    async def list_filters(self) -> list[Filter]:
        async with aiosqlite.connect(self.path) as db:
            cur = await db.execute(
                "SELECT id, name, brand, category, size, max_price, keywords FROM filters"
            )
            rows = await cur.fetchall()
            return [
                Filter(
                    id=row[0],
                    name=row[1],
                    brand=row[2],
                    category=row[3],
                    size=row[4],
                    max_price=row[5],
                    keywords=json.loads(row[6] or "[]"),
                )
                for row in rows
            ]

    # ---------- settings k/v ----------

    async def set_setting(self, key: str, value: str) -> None:
        async with aiosqlite.connect(self.path) as db:
            await db.execute(
                "INSERT INTO settings(key, value) VALUES (?, ?) "
                "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
                (key, value),
            )
            await db.commit()

    async def get_setting(self, key: str, default: str | None = None) -> str | None:
        async with aiosqlite.connect(self.path) as db:
            cur = await db.execute("SELECT value FROM settings WHERE key = ?", (key,))
            row = await cur.fetchone()
            return row[0] if row else default

    # ---------- stats ----------

    async def incr_stat(self, key: str, by: int = 1) -> None:
        async with aiosqlite.connect(self.path) as db:
            await db.execute(
                "INSERT INTO stats(key, value) VALUES (?, ?) "
                "ON CONFLICT(key) DO UPDATE SET value = value + ?",
                (key, by, by),
            )
            await db.commit()

    async def get_stat(self, key: str) -> int:
        async with aiosqlite.connect(self.path) as db:
            cur = await db.execute("SELECT value FROM stats WHERE key = ?", (key,))
            row = await cur.fetchone()
            return int(row[0]) if row else 0
