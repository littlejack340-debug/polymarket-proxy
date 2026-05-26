"""Slash commands admin : stats, pause, resume."""
from __future__ import annotations

import time
from datetime import timedelta

import discord
from discord import app_commands
from discord.ext import commands

from database import Database


class AdminCog(commands.Cog):
    def __init__(self, bot: commands.Bot, db: Database, started_at: float) -> None:
        self.bot = bot
        self.db = db
        self.started_at = started_at

    @app_commands.command(name="stats", description="Affiche les stats du bot.")
    async def stats(self, interaction: discord.Interaction) -> None:
        posted = await self.db.get_stat("deals_posted")
        scanned = await self.db.get_stat("items_scanned")
        filters = await self.db.list_filters()
        paused = (await self.db.get_setting("paused", "0")) == "1"
        uptime = timedelta(seconds=int(time.time() - self.started_at))

        embed = discord.Embed(
            title="📊 Vinted Resell Bot — Stats",
            color=discord.Color.blurple(),
        )
        embed.add_field(name="Uptime", value=str(uptime), inline=True)
        embed.add_field(name="Etat", value="⏸️ pause" if paused else "▶️ actif", inline=True)
        embed.add_field(name="Filtres actifs", value=str(len(filters)), inline=True)
        embed.add_field(name="Annonces scannees", value=str(scanned), inline=True)
        embed.add_field(name="Deals postes", value=str(posted), inline=True)
        await interaction.response.send_message(embed=embed, ephemeral=True)

    @app_commands.command(name="pause", description="Met la surveillance en pause.")
    async def pause(self, interaction: discord.Interaction) -> None:
        await self.db.set_setting("paused", "1")
        await interaction.response.send_message("⏸️ Surveillance en pause.", ephemeral=True)

    @app_commands.command(name="resume", description="Relance la surveillance.")
    async def resume(self, interaction: discord.Interaction) -> None:
        await self.db.set_setting("paused", "0")
        await interaction.response.send_message("▶️ Surveillance relancee.", ephemeral=True)


async def setup(bot: commands.Bot, db: Database, started_at: float) -> None:
    await bot.add_cog(AdminCog(bot, db, started_at))
