-- Table des inscriptions
create table if not exists public.registrations (
  id uuid primary key default gen_random_uuid(),
  first_name text not null,
  last_name text not null,
  email text not null,
  generated_image_url text,
  created_at timestamptz not null default now()
);

-- Bucket public pour les images récapitulatives.
-- À exécuter une fois (le bucket doit être public pour servir l'URL renvoyée par l'API).
insert into storage.buckets (id, name, public)
values ('registrations', 'registrations', true)
on conflict (id) do nothing;
