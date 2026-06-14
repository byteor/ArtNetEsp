// Minifies the non-SPA web assets (the captive-portal page and the shared
// stylesheet) with node tooling, overwriting the verbatim copies Vite already
// placed in ../data/www from web/public/. Run after `vite build` (package.json).
//
// portal.html keeps its runtime {{HOST}}/{{VERSION}}/{{IP}}/{{NETWORKS}}
// placeholders (substituted by the firmware's Connect::portalPage()), so they
// are protected from the minifier via ignoreCustomFragments.

import { minify } from "html-minifier-terser";
import * as esbuild from "esbuild";
import { readFile, writeFile, mkdir } from "node:fs/promises";
import { fileURLToPath } from "node:url";

const here = (p) => fileURLToPath(new URL(p, import.meta.url));
const out = (p) => here(`../../data/www/${p}`);

await mkdir(here("../../data/www"), { recursive: true });

// style.css -> minified
const cssSrc = await readFile(here("../public/style.css"), "utf8");
const css = await esbuild.transform(cssSrc, { loader: "css", minify: true });
await writeFile(out("style.css"), css.code);
console.log(`build-static: style.css  ${cssSrc.length} -> ${css.code.length} bytes`);

// portal.html -> minified (placeholders preserved; inline JS mangled)
const portalSrc = await readFile(here("../public/portal.html"), "utf8");
const portal = await minify(portalSrc, {
  collapseWhitespace: true,
  removeComments: true,
  minifyCSS: true,
  minifyJS: true,
  ignoreCustomFragments: [/\{\{[\s\S]*?\}\}/],
});
await writeFile(out("portal.html"), portal);
console.log(`build-static: portal.html ${portalSrc.length} -> ${portal.length} bytes`);
