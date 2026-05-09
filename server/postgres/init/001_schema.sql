CREATE SCHEMA IF NOT EXISTS auracloud;

CREATE TABLE IF NOT EXISTS auracloud.users (
  email TEXT PRIMARY KEY,
  salt TEXT NOT NULL,
  password_hash TEXT NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS auracloud.user_settings (
  email TEXT PRIMARY KEY REFERENCES auracloud.users(email) ON DELETE CASCADE,
  system_prompts JSONB NOT NULL DEFAULT '[]'::jsonb,
  mcp_presets JSONB NOT NULL DEFAULT '{}'::jsonb,
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

