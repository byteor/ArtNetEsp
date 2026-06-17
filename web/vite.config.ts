import { defineConfig } from "vite";
import preact from "@preact/preset-vite";

// During `npm run dev`, REST calls are proxied to a live device; override with
// e.g. `DEVICE=http://192.168.4.26 npm run dev`. The static assets in
// web/public/ (style.css, portal.html) are served by Vite automatically.
const DEVICE = process.env.DEVICE || "http://Art-a3da3bd8.local";
const apiPaths = ["/config", "/status", "/heap", "/reboot", "/reset-wifi", "/update"];

export default defineConfig({
  plugins: [preact()],
  base: "/",
  build: {
    outDir: "../data/www",
    emptyOutDir: false, // build-static.mjs overwrites the public copies; don't wipe
    target: "es2018",
    rollupOptions: {
      output: {
        inlineDynamicImports: true, // single app.js, no code-split chunks
        entryFileNames: "app.js",
        assetFileNames: "app.[ext]",
        chunkFileNames: "app-[name].js",
      },
    },
  },
  server: {
    proxy: Object.fromEntries(
      apiPaths.map((p) => [p, { target: DEVICE, changeOrigin: true }]),
    ),
  },
});
