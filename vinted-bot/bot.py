"""Point d'entree du bot Discord Vinted Resell.

Lance la boucle de surveillance Vinted en parallele du client Discord.
"""
from __future__ import annotations

import asyncio
import logging
import sys
import time

import discord
from discord.ext import commands

from commands import admin as admin_cog
from commands import filters as filters_cog
from config import SETTINGS
from database import Database, Filter
from deal_scorer import evaluate
from embeds import build_deal_embed
from vinted_scraper import VintedItem, VintedScraper

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger("vinted-bot")


class VintedBot(commands.Bot):
    def __init__(self, db: Database, scraper: VintedScraper, started_at: float) -> None:
        intents = discord.Intents.default()
        super().__init__(command_prefix="!", intents=intents)
        self.db = db
        self.scraper = scraper
        self.started_at = started_at
        self._poll_task: asyncio.Task | None = None

    async def setup_hook(self) -> None:
        # Enregistre les cogs
        await filters_cog.setup(self, self.db)
        await admin_cog.setup(self, self.db, self.started_at)

        # Sync des slash commands sur la guild (instantane)
        if SETTINGS.guild_id:
            guild = discord.Object(id=SETTINGS.guild_id)
            self.tree.copy_global_to(guild=guild)
            synced = await self.tree.sync(guild=guild)
            logger.info("Synced %d slash commands on guild %s", len(synced), SETTINGS.guild_id)
        else:
            synced = await self.tree.sync()
            logger.info("Synced %d global slash commands", len(synced))

        # Demarre la boucle de polling Vinted
        self._poll_task = asyncio.create_task(self._poll_loop(), name="vinted-poll-loop")

    async def on_ready(self) -> None:
        logger.info("Logged in as %s (id=%s)", self.user, self.user.id if self.user else "?")

    async def close(self) -> None:
        if self._poll_task:
            self._poll_task.cancel()
        await self.scraper.close()
        await super().close()

    # ---------- boucle de surveillance ----------

    async def _poll_loop(self) -> None:
        await self.wait_until_ready()
        logger.info("Polling loop started (interval=%ss)", SETTINGS.poll_interval)

        while not self.is_closed():
            try:
                paused = (await self.db.get_setting("paused", "0")) == "1"
                if paused:
                    await asyncio.sleep(SETTINGS.poll_interval)
                    continue

                filters = await self.db.list_filters()
                if not filters:
                    await asyncio.sleep(SETTINGS.poll_interval)
                    continue

                for flt in filters:
                    await self._process_filter(flt)

                # On nettoie regulierement la table des items vus
                await self.db.prune_seen()
            except asyncio.CancelledError:
                raise
            except Exception:  # noqa: BLE001
                logger.exception("Unexpected error in poll loop")

            await asyncio.sleep(SETTINGS.poll_interval)

    async def _process_filter(self, flt: Filter) -> None:
        logger.debug("Processing filter '%s'", flt.name)
        try:
            items = await self.scraper.search(flt)
        except Exception:  # noqa: BLE001
            logger.exception("Search failed for filter '%s'", flt.name)
            return

        for item in items:
            if not item.item_id:
                continue
            await self.db.incr_stat("items_scanned")

            if await self.db.is_seen(item.item_id):
                continue

            try:
                evaluation = await evaluate(self.scraper, item)
            except Exception:  # noqa: BLE001
                logger.exception("Scoring failed for item %s", item.item_id)
                continue

            # Seuil minimal pour eviter de spammer le salon
            if evaluation.score < 3:
                await self.db.mark_seen(item.item_id, evaluation.score)
                continue

            await self._post_deal(item, evaluation)
            await self.db.mark_seen(item.item_id, evaluation.score)
            await self.db.incr_stat("deals_posted")

    async def _post_deal(self, item: VintedItem, evaluation) -> None:
        channel = await self._resolve_channel()
        if channel is None:
            logger.warning("No destination channel configured, skipping deal %s", item.item_id)
            return

        embed = build_deal_embed(item, evaluation)
        content = self._build_ping(evaluation.score)
        try:
            await channel.send(content=content or None, embed=embed)
        except discord.DiscordException:
            logger.exception("Failed to post deal %s", item.item_id)

    async def _resolve_channel(self) -> discord.abc.Messageable | None:
        stored = await self.db.get_setting("channel_id")
        channel_id = int(stored) if stored else SETTINGS.default_channel_id
        if not channel_id:
            return None
        channel = self.get_channel(channel_id)
        if channel is None:
            try:
                channel = await self.fetch_channel(channel_id)
            except discord.DiscordException:
                logger.exception("Cannot fetch channel %s", channel_id)
                return None
        return channel

    def _build_ping(self, score: int) -> str:
        # Ping selon le score (specifie dans le prompt)
        if score >= 5:
            return "@everyone"
        if score >= 4 and SETTINGS.resellers_role_id:
            return f"<@&{SETTINGS.resellers_role_id}>"
        return ""


async def main() -> None:
    if not SETTINGS.discord_token:
        logger.error("DISCORD_TOKEN manquant — voir .env.example")
        sys.exit(1)

    db = Database()
    await db.init()

    scraper = VintedScraper()
    await scraper.start()

    bot = VintedBot(db=db, scraper=scraper, started_at=time.time())
    try:
        await bot.start(SETTINGS.discord_token)
    finally:
        await scraper.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Shutting down (keyboard interrupt)")
