#!/usr/bin/env python3
"""v11 wrapper for the receiver failure audit.

Historical audit semantics are left unchanged. For v11 only, an independently
audited `resolved_converged_target` row is a non-failure alongside the legacy
single-vtable resolved statuses.
"""
from __future__ import annotations

import audit_nevergone_receiver_type_failures as base

base.RESOLVED.add("resolved_converged_target")
base.NON_FAILURE.add("resolved_converged_target")

if __name__ == "__main__":
    raise SystemExit(base.main())
