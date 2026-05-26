"""Slash commands liees a la gestion des filtres et du salon de sortie."""
from __future__ import annotations

import discord
from discord import app_commands
from discord.ext import commands

from database import Database


class FiltersCog(commands.Cog):
    def __init__(self, bot: commands.Bot, db: Database) -> None:
        self.bot = bot
        self.db = db

    @app_commands.command(
        name="add_filter",
        description="Ajoute un filtre de surveillance Vinted.",
    )
    @app_commands.describe(
        name="Nom unique du filtre",
        brand="Marque recherchee (optionnel)",
        category="ID de catalogue Vinted (optionnel)",
        size="Taille (optionnel)",
        max_price="Prix max en EUR (optionnel)",
        keywords="Mots-cles separes par des virgules (optionnel)",
    )
    async def add_filter(
        self,
        interaction: discord.Interaction,
        name: str,
        brand: str | None = None,
        category: str | None = None,
        size: str | None = None,
        max_price: float | None = None,
        keywords: str | None = None,
    ) -> None:
        kw_list = (
            [k.strip() for k in keywords.split(",") if k.strip()]
            if keywords else []
        )
        try:
            flt = await self.db.add_filter(
                name=name,
                brand=brand,
                category=category,
                size=size,
                max_price=max_price,
                keywords=kw_list,
            )
        except Exception as exc:  # noqa: BLE001
            await interaction.response.send_message(
                f"❌ Impossible d'ajouter le filtre `{name}` : {exc}",
                ephemeral=True,
            )
            return
        await interaction.response.send_message(
            f"✅ Filtre **{flt.name}** (#{flt.id}) ajoute.", ephemeral=True
        )

    @app_commands.command(
        name="remove_filter",
        description="Supprime un filtre par son nom.",
    )
    @app_commands.describe(name="Nom du filtre a supprimer")
    async def remove_filter(self, interaction: discord.Interaction, name: str) -> None:
        ok = await self.db.remove_filter(name)
        if ok:
            await interaction.response.send_message(
                f"🗑️ Filtre **{name}** supprime.", ephemeral=True
            )
        else:
            await interaction.response.send_message(
                f"⚠️ Aucun filtre nomme `{name}`.", ephemeral=True
            )

    @app_commands.command(
        name="list_filters",
        description="Liste les filtres actifs.",
    )
    async def list_filters(self, interaction: discord.Interaction) -> None:
        filters = await self.db.list_filters()
        if not filters:
            await interaction.response.send_message(
                "Aucun filtre actif. Utilise `/add_filter` pour en creer un.",
                ephemeral=True,
            )
            return

        embed = discord.Embed(
            title=f"Filtres actifs ({len(filters)})",
            color=discord.Color.blurple(),
        )
        for f in filters:
            value_lines = []
            if f.brand:
                value_lines.append(f"Marque : {f.brand}")
            if f.category:
                value_lines.append(f"Catalogue : {f.category}")
            if f.size:
                value_lines.append(f"Taille : {f.size}")
            if f.max_price is not None:
                value_lines.append(f"Prix max : {f.max_price} EUR")
            if f.keywords:
                value_lines.append(f"Mots-cles : {', '.join(f.keywords)}")
            embed.add_field(
                name=f"#{f.id} — {f.name}",
                value="\n".join(value_lines) or "(aucun critere)",
                inline=False,
            )
        await interaction.response.send_message(embed=embed, ephemeral=True)

    @app_commands.command(
        name="set_channel",
        description="Definit le salon ou seront postees les alertes.",
    )
    @app_commands.describe(channel="Le salon Discord cible")
    async def set_channel(
        self, interaction: discord.Interaction, channel: discord.TextChannel
    ) -> None:
        await self.db.set_setting("channel_id", str(channel.id))
        await interaction.response.send_message(
            f"📌 Salon de destination : {channel.mention}", ephemeral=True
        )


async def setup(bot: commands.Bot, db: Database) -> None:
    await bot.add_cog(FiltersCog(bot, db))
