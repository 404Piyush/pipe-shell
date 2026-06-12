# Deployment — pipe-shell site

This repo ships with a static landing page under `site/`
and a `netlify.toml` that auto-deploys it.

## One-time setup

1. Go to https://app.netlify.com/start and click
   **"Import an existing project"**.
2. Pick **GitHub** → **404Piyush/pipe-shell**.
3. Netlify will auto-detect the `netlify.toml`.  Confirm:
   - Build command: *(empty)*
   - Publish directory: `site`
4. Click **Deploy site**.  You'll get a random
   `*.netlify.app` URL in ~30 seconds.

## Custom domain: pipe-shell.404piyush.me

In Netlify → **Site settings** → **Domain management** →
**Add custom domain** → `pipe-shell.404piyush.me`.

Netlify will show you the DNS record to add.  Go to your
DNS provider for `404piyush.me` and add:

```
Type:  CNAME
Name:  pipe-shell
Value: <your-site>.netlify.app
```

(Or an ALIAS/ANAME if your provider doesn't support
CNAME at the apex.)

Netlify will auto-provision a Let's Encrypt certificate.
First deploy on the custom domain: ~5 minutes after the
DNS record propagates.

## Verify

```
curl -I https://pipe-shell.404piyush.me
```

Should return `200 OK` with a `Strict-Transport-Security`
header (proves HTTPS is on).

## Continuous deploy

Every push to `master` redeploys the site automatically.
No further action needed.

## Local preview

```sh
cd site
python3 -m http.server 8000
# open http://localhost:8000
```
