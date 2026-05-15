const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
const STORAGE_BUCKET = "registrations";

export default async function handler(req, res) {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type");

  if (req.method === "OPTIONS") return res.status(200).end();
  if (req.method !== "POST") {
    return res.status(405).json({ error: "Méthode non autorisée." });
  }

  try {
    const body = typeof req.body === "string" ? JSON.parse(req.body || "{}") : (req.body || {});
    const { firstName, lastName, email, password, imageBase64 } = body;

    if (!firstName || !lastName || !email || !password) {
      return res.status(400).json({ error: "Veuillez remplir tous les champs." });
    }
    if (!EMAIL_RE.test(email)) {
      return res.status(400).json({ error: "Adresse email invalide." });
    }
    if (String(password).length < 8) {
      return res.status(400).json({ error: "Le mot de passe doit contenir au moins 8 caractères." });
    }
    if (!imageBase64 || !imageBase64.startsWith("data:image/")) {
      return res.status(400).json({ error: "Image récapitulative manquante." });
    }

    const SUPABASE_URL = process.env.SUPABASE_URL;
    const SERVICE_KEY = process.env.SUPABASE_SERVICE_ROLE_KEY;

    // Sans Supabase configuré : l'inscription est validée et l'image
    // récapitulative (générée côté application) est renvoyée au client.
    if (!SUPABASE_URL || !SERVICE_KEY) {
      return res.status(200).json({ ok: true, demo: true, imageUrl: null });
    }

    const authHeaders = {
      apikey: SERVICE_KEY,
      Authorization: `Bearer ${SERVICE_KEY}`,
    };

    // 1. Supabase Auth : création de l'utilisateur
    const authRes = await fetch(`${SUPABASE_URL}/auth/v1/admin/users`, {
      method: "POST",
      headers: { ...authHeaders, "Content-Type": "application/json" },
      body: JSON.stringify({
        email,
        password,
        email_confirm: true,
        user_metadata: { first_name: firstName, last_name: lastName },
      }),
    });
    if (!authRes.ok) {
      const detail = await authRes.json().catch(() => ({}));
      const msg = detail.msg || detail.error_description || detail.message || "";
      if (authRes.status === 422 || /already/i.test(msg)) {
        return res.status(409).json({ error: "Cet email est déjà inscrit." });
      }
      throw new Error(`Auth: ${msg || authRes.status}`);
    }

    // 2. Storage : upload de l'image récapitulative
    const base64 = imageBase64.split(",")[1];
    const buffer = Buffer.from(base64, "base64");
    const objectPath = `recaps/${Date.now()}-${email.replace(/[^a-z0-9]/gi, "_")}.png`;

    const uploadRes = await fetch(
      `${SUPABASE_URL}/storage/v1/object/${STORAGE_BUCKET}/${objectPath}`,
      {
        method: "POST",
        headers: { ...authHeaders, "Content-Type": "image/png", "x-upsert": "true" },
        body: buffer,
      }
    );
    if (!uploadRes.ok) {
      const detail = await uploadRes.text().catch(() => "");
      throw new Error(`Storage: ${detail || uploadRes.status}`);
    }
    const imageUrl = `${SUPABASE_URL}/storage/v1/object/public/${STORAGE_BUCKET}/${objectPath}`;

    // 3. Base de données : enregistrement dans la table registrations
    const insertRes = await fetch(`${SUPABASE_URL}/rest/v1/registrations`, {
      method: "POST",
      headers: {
        ...authHeaders,
        "Content-Type": "application/json",
        Prefer: "return=representation",
      },
      body: JSON.stringify({
        first_name: firstName,
        last_name: lastName,
        email,
        generated_image_url: imageUrl,
      }),
    });
    if (!insertRes.ok) {
      const detail = await insertRes.text().catch(() => "");
      throw new Error(`DB: ${detail || insertRes.status}`);
    }

    return res.status(200).json({ ok: true, imageUrl });
  } catch (error) {
    return res.status(500).json({ error: error.message });
  }
}
