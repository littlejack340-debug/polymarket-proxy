"""Construction des embeds Discord pour les annonces detectees."""
from __future__ import annotations

import time
from datetime import timedelta

import discord

from deal_scorer import DealEvaluation
from vinted_scraper import VintedItem


def _format_age(created_at_ts: int | None) -> str:
    if not created_at_ts:
        return "n/a"
    delta = max(0, int(time.time() - created_at_ts))
    td = timedelta(seconds=delta)
    if td < timedelta(minutes=1):
        return f"{delta}s"
    if td < timedelta(hours=1):
        return f"{delta // 60} min"
    if td < timedelta(days=1):
        return f"{delta // 3600} h"
    return f"{delta // 86400} j"


def _stars(score: int) -> str:
    score = max(0, min(score, 5))
    return "⭐" * score + "·" * (5 - score)


def _embed_color(score: int) -> discord.Color:
    if score >= 5:
        return discord.Color.gold()
    if score >= 4:
        return discord.Color.green()
    if score >= 3:
        return discord.Color.blue()
    return discord.Color.dark_grey()


def build_deal_embed(item: VintedItem, evaluation: DealEvaluation) -> discord.Embed:
    embed = discord.Embed(
        title=item.title[:256] or "(sans titre)",
        url=item.url,
        color=_embed_color(evaluation.score),
    )

    if item.photo_url:
        embed.set_image(url=item.photo_url)

    embed.add_field(name="Marque", value=item.brand or "—", inline=True)
    embed.add_field(name="Taille", value=item.size or "—", inline=True)
    embed.add_field(name="Etat", value=item.status or "—", inline=True)

    price_str = f"**{item.price:.2f} {item.currency}**"
    if evaluation.market_price > 0:
        price_str += f"\nMarche : ~{evaluation.market_price:.2f} {item.currency}"
        if evaluation.sample_size:
            price_str += f" (n={evaluation.sample_size})"
    embed.add_field(name="Prix", value=price_str, inline=True)

    if evaluation.market_price > 0:
        margin_str = (
            f"+{evaluation.margin_eur:.2f} {item.currency}\n"
            f"({evaluation.margin_pct:.0f} %)"
        )
    else:
        margin_str = "marche inconnu"
    embed.add_field(name="Marge potentielle", value=margin_str, inline=True)

    embed.add_field(
        name="Fraicheur", value=f"il y a {_format_age(item.created_at_ts)}", inline=True
    )

    embed.add_field(
        name="Score de deal",
        value=f"{_stars(evaluation.score)}  {evaluation.score}/5",
        inline=False,
    )

    if item.seller_login:
        embed.set_footer(text=f"Vendeur : {item.seller_login}")

    return embed
