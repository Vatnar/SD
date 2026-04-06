---
title: Memory Management
date: 2026-04-02
status: draft
summary: Custom allocator strategies
---

# Memory Management

## Goals
- Arena allocators for game subsystems
- Stack allocation for temporary data
- Pool allocators for frequent small allocations

## Approach
Use custom allocators to reduce allocation overhead and improve cache locality.
