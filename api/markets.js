export default async function handler(req, res) {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type");

  if (req.method === "OPTIONS") {
    return res.status(200).end();
  }

  try {
    const response = await fetch(
      "https://gamma-api.polymarket.com/markets?active=true&closed=false&limit=50&order=volume24hr&ascending=false",
      { headers: { "Accept": "application/json", "User-Agent": "Mozilla/5.0" } }
    );

    if (!response.ok) throw new Error(`API error: ${response.status}`);

    const data = await response.json();

    const markets = (Array.isArray(data) ? data : data.markets || [])
      .filter(m => m.question && m.outcomePrices)
      .map(m => {
        let prices = [];
        try { prices = JSON.parse(m.outcomePrices); } catch { prices = ["0.5", "0.5"]; }
        const yesPrice = parseFloat(prices[0]) || 0.5;
        return {
          id: m.id || m.conditionId,
          question: m.question,
          yesPrice: +yesPrice.toFixed(3),
          noPrice: +(1 - yesPrice).toFixed(3),
          volume: parseFloat(m.volume24hr || m.volume || 0),
          endDate: m.endDateIso || m.endDate || "",
          category: m.tags?.[0]?.label || "Other",
        };
      })
      .filter(m => m.yesPrice > 0.02 && m.yesPrice < 0.98);

    res.setHeader("Cache-Control", "s-maxage=30, stale-while-revalidate");
    return res.status(200).json({ markets, updatedAt: new Date().toISOString() });

  } catch (error) {
    return res.status(500).json({ error: error.message });
  }
}
