# Thiết kế mục tiêu Unified Colab an toàn và có thể đo kiểm

## Problem

Unified Colab hiện giúp Desktop dùng một base URL cho nhiều exact worker, nhưng
coordinator khởi chạy nhiều process/model và giữ chúng resident. Điều này chưa
chứng minh được budget VRAM, lifecycle sau OOM, lease giữa các request, hay hành vi
khi Colab/tunnel reset. Tài liệu cũ cũng dễ bị hiểu thành zero-config, tám task AI
trên Colab và không bao giờ OOM.

## Decision

Giữ coordinator/proxy hiện tại làm optional remote route, nhưng chuyển sang manifest
versioned + exact worker + lazy activation + một GPU lease trong profile
`SAFE_T4`. Desktop phân biệt inventory/configured với live verified/ready. Local
route vẫn là đường chạy độc lập cho ingest, normalize, mix và export. Không silent
fallback khi Direct Colab đã được chọn.

## Non-goals

- Không bảo đảm Colab luôn cấp GPU, loại GPU hoặc uptime.
- Không biến mọi model thành một monolithic inference server.
- Không tự động phát hiện hostname hoặc token qua mạng công cộng.
- Không dùng `empty_cache()` như cơ chế chống OOM duy nhất.
- Không build/package EXE trong pha thiết kế.

## Contract invariants

1. Mọi inference route phải có capability, model, variant, worker revision và
   response contract exact.
2. Mọi request GPU phải có lease TTL, correlation id và cleanup trong `finally`.
3. `Ready` chỉ được hiển thị sau live health/capabilities của stage đang lease.
4. Artifact chỉ được commit sau checksum/schema/media validation.
5. Technical error được giữ trong log; UI hiển thị hướng dẫn và CTA không lộ secret.
6. Mọi thay đổi phải có unit/contract/fault test và rollback bằng feature flag.
