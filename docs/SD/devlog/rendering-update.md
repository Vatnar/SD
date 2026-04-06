---
title: Rendering Pipeline Update
date: 2026-04-05
tags: ["rendering", "graphics"]
summary: New rendering architecture
---

# Rendering Pipeline Update

Working on a new render graph system. Current status:
- Command buffer system working
- Frame graph in progress
- Want to add batching

![Render Graph](/devlog-images/render-graph.png)

## Architecture

The render graph uses nodes and edges to manage rendering passes.
