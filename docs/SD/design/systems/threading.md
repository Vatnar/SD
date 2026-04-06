---
title: Threading Model
date: 2026-04-03
status: review
summary: Job system design
---

# Threading Model

## Job System
- Worker thread pool
- Job dependencies
- Fiber-based task system

## Sync Primitives
- Atomics for simple counters
- Lock-free queues for job queues
- Mutexes only where needed
