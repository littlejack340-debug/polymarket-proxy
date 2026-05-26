# Vinted Resell Bot

Bot Discord en Python qui surveille Vinted en temps reel, score les annonces et
poste automatiquement les meilleurs deals dans un salon Discord.

## Fonctionnalites

- Surveillance asynchrone de Vinted via son API non officielle
- Filtres configurables (marque, categorie, taille, prix max, mots-cles)
- Detection des annonces sous-cotees vs prix marche median
- Score de deal 0..5 avec ping conditionnel (`@everyone` >= 5, `@resellers` >= 4)
- Embeds Discord riches (photo, marge, fraicheur, score)
- Dedup persistant en SQLite
- Slash commands pour gerer les filtres / le salon / la pause
- Rotation de User-Agent + backoff exponentiel pour limiter les bans IP
- Reconnexion Discord automatique (gere par `discord.py`)

## Architecture

```
vinted-bot/
├── bot.py               # Point d'entree, boucle de polling
├── config.py            # Variables d'env et constantes
├── vinted_scraper.py    # Client HTTP Vinted
├── deal_scorer.py       # Estimation marche + score de deal
├── database.py          # SQLite (deals vus, filtres, stats)
├── embeds.py            # Embeds Discord
├── commands/
│   ├── filters.py       # /add_filter, /remove_filter, /list_filters, /set_channel
│   └── admin.py         # /stats, /pause, /resume
├── .env.example
├── requirements.txt
└── README.md
```

## Installation

```bash
cd vinted-bot
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
# Editer .env avec ton DISCORD_TOKEN, GUILD_ID, DEFAULT_CHANNEL_ID
```

### Creer l'app Discord

1. https://discord.com/developers/applications -> New Application
2. Onglet "Bot" -> Add Bot -> recupere le token (a coller dans `.env`)
3. Onglet "OAuth2 > URL Generator" -> scopes `bot` + `applications.commands`,
   permissions au minimum : `Send Messages`, `Embed Links`, `Mention Everyone`
   (si tu veux pinger sur score 5/5)
4. Invite le bot sur ton serveur via l'URL generee

### Lancement

```bash
python bot.py
```

Au demarrage, les slash commands sont synchronisees sur la guild definie par
`GUILD_ID` (effet immediat). Sans `GUILD_ID`, la sync globale peut prendre
jusqu'a 1h cote Discord.

## Utilisation Discord

```
/add_filter name:nike_air_max brand:Nike keywords:air,max max_price:60
/add_filter name:carhartt_xl  brand:Carhartt size:XL max_price:40
/list_filters
/set_channel channel:#deals
/stats
/pause
/resume
/remove_filter name:nike_air_max
```

## Score de deal

| Critere                              | Points |
|--------------------------------------|--------|
| Marge >= 50% vs prix marche estime   | +2     |
| Marge >= 30% (sans cumul avec +2)    | +1     |
| Etat "neuf" / "neuf avec etiquette"  | +1     |
| Marque premium (Nike, Adidas, ...)   | +1     |
| Annonce mise en ligne il y a < 10min | +1     |

Score plafonne a 5. Seuls les items >= 3 sont postes.

## Extensibilite

Le client `VintedScraper` est isole dans `vinted_scraper.py`. Pour ajouter une
nouvelle plateforme (Depop, Vestiaire Collective), il suffit de creer un
client similaire qui retourne des `VintedItem` (ou une dataclass equivalente)
et de l'ajouter a la boucle de polling dans `bot.py`.

## Notes importantes

- Vinted n'expose pas d'API publique. L'endpoint utilise (`/api/v2/catalog/items`)
  peut changer sans preavis. Si tu vois beaucoup de `401`/`403`, c'est probable
  qu'il faille mettre a jour le User-Agent ou les params.
- Respecte les CGU de Vinted. Ce bot est fourni a titre educatif.
- Le delai par defaut entre 2 requetes HTTP est de 4 secondes. Ne le baisse
  pas en dessous de 3s pour eviter les bans IP.
