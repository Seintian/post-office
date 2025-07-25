# Pull template

## Description

- **What** changed and **why**?  
- Which issue or feature does this address?

## Changes

- List of major changes:
  - `src/net/poller.c`: added edge-triggered epoll support.
  - `include/postoffice/net.h`: new API `po_set_nonblocking()`
  - Updated tests: `tests/net/poller_test.c`

## Verification

- [ ] Built locally with no warnings (`make all` passes).  
- [ ] Unit tests added/updated and all pass (`make test`).  
- [ ] Integration smoke test: echo server ↔ client works.  
- [ ] Performance baseline unchanged or improved (attach numbers).

## Checklist

- [ ] Code follows style guide (`clang-format` applied).  
- [ ] No TODOs or commented‑out code left.  
- [ ] All public APIs documented in header comments.  
- [ ] Changelog entry added (if applicable).  
