---
date: 2026-04-07
---
A SparseEntitySet, or more accurately, an entity set of sparse pages mapped to dense arrays was chosen to maximise flexibility while allowing for sequential access of components. The components themselves, which are pure data structs, are stored sequentially in dense arrays, allowing for fast lookup by systems querying the ECS.

We set each page size to 1024 (number of entries). So we need $\text{log}_2(1024)=10$ bits to store the page offset. The remaining bits then describe on which page that index is on. We choose to use the lower 10 bits of the 32 bit index to store the offset. And the higher 22 bits for which page it is.

Therefore to extract on what page the index is on, and on what offset from the start of that page we do:
```cpp
auto page = index >> 10; // discard the "offset" bits
auto offset = index & (1023); // (2^10 - 1), discard the "page" bits
```
Since entity indexes are assigned incrementally, we then only need to assign storage for 1024 entries at a time. Keeping us storage efficient.

Conceptually, this is a sparse array of arrays: the outer array maps to pages, while each inner array stores indices into the dense array for that page.
```cpp
usize denseIdx = sparse[page][offset];
auto& value = denseData[dense_idx];
```

This allows for efficient memory usage and a flexible component system. While maintaining fast sequential lookup for systems.
