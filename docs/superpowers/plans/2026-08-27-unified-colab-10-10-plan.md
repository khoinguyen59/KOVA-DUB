# Kế hoạch tối ưu Unified Colab Worker — mục tiêu khả thi 10/10

> Ngày: 2026-08-27
> Phạm vi: `LA-Studio` Desktop C++/Qt/QML và notebook/coordinator Colab
> Loại tài liệu: kế hoạch triển khai có cổng nghiệm thu, không phải cam kết GPU bên ngoài
> Proposal nền: `C:\Users\Nguyen Trong Khoi\Downloads\TTS\doc\unified_colab_worker_proposal.md`

**Mục tiêu:** biến route Unified Colab thành một đường chạy remote có xác thực,
đúng contract, chịu được runtime reset và không làm hỏng route Local/API. Mục tiêu
10/10 được hiểu là kế hoạch có phạm vi hữu hạn, số đo, test hồi quy, cơ chế dừng
an toàn và rollback rõ ràng. Không coi GPU Colab, quota, thời gian sống runtime,
uptime tunnel hoặc mức VRAM là thứ phần mềm có thể bảo đảm tuyệt đối.

**Kiến trúc chốt:** Desktop nhập một URL HTTPS và bearer token một lần. Coordinator
chỉ làm gateway/authentication/orchestration; inference vẫn chạy trong exact worker
của từng capability. `SAFE_T4` dùng một GPU lease duy nhất và chỉ giữ model của
stage đang xử lý; không khởi động sẵn toàn bộ model. Stage local (ingest,
normalize, mix, export) vẫn chạy local. Khi Colab không sẵn sàng, UI chỉ báo lỗi có
hướng dẫn; không tự động đổi sang Local/API nếu người dùng đã chọn Direct Colab.

**Spec:** nội dung thiết kế và các tiêu chí trong chính tài liệu này; các contract
hiện hành được đối chiếu với `doc/unified_colab_worker_proposal.md` và code thật
trước khi thực thi.

## 1. Các sự thật đã được kiểm chứng

| Sự thật | Hệ quả thiết kế |
|---|---|
| `DubbingController::connectUnifiedWorkflowColab(workerUrl, bearerToken)` đã có trong `src/controllers/dubbing/DubbingController.h:417-421`; implementation kiểm token, stage được chọn và rollback trong `src/controllers/dubbing/parts/DubbingController_Colab.cpp:303-392`. | Giữ API public hiện tại, mở rộng từng bước; không tạo API giả hoặc route tự suy đoán. |
| `unifiedColabStageUrl()` tạo `/v1/unified/<capability>/<model>` và notebook coordinator proxy tiếp vào exact worker. | Mọi model phải có capability/model/route/response contract cụ thể. |
| `ColabSession` kiểm `/health` và `/v1/capabilities`, yêu cầu contract version, CUDA, exact model/variant, `loaded` và response contract. | Không được báo Ready theo việc tunnel còn sống; phải phân biệt configured, leased, ready, failed. |
| `LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py` hiện start worker tuần tự nhưng giữ nhiều process/model resident. | Cần GPU scheduler/lease và lifecycle stop/restart; tuần tự khởi động không đồng nghĩa tuần tự dùng VRAM. |
| Pipeline hiện chọn một số exact worker: voice isolation, STT, OCR, translation, TTS, alignment; ingest/normalize/mix/export không phải remote AI worker. | Không mô tả là “8 task AI trên Colab”; chỉ khai báo remote capabilities thực tế. |
| Colab công bố tài nguyên không được đảm bảo, không vô hạn và giới hạn sử dụng có thể dao động; các hoạt động kiểu bypass notebook UI hoặc distributed workers có thể bị hạn chế trong tier miễn phí. | Không dùng “100% free”, “always available”, “zero-config”, “never OOM” làm acceptance criterion. Xem [Colab FAQ](https://research.google.com/colaboratory/intl/en-GB/faq.html). |
| Cloudflare Quick Tunnel tạo hostname ngẫu nhiên, không có SLA/uptime guarantee, dành cho test/dev, giới hạn 200 request in-flight và không hỗ trợ SSE. | Dùng cho development/preview; production cần managed tunnel hoặc runtime do người dùng kiểm soát. Không dùng SSE và phải xử lý 429. Xem [Cloudflare Quick Tunnels](https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/do-more-with-tunnels/trycloudflare/). |
| PyTorch `empty_cache()` chỉ giải phóng cached memory chưa dùng; tensor còn giữ reference vẫn không được giải phóng. `mem_get_info`, `memory_allocated`, `memory_reserved` dùng để đo. | `empty_cache()` chỉ là cleanup best-effort; bắt buộc process isolation/lease/measurement. Xem [PyTorch CUDA semantics](https://docs.pytorch.org/docs/stable/notes/cuda.html) và [CUDA API](https://docs.pytorch.org/docs/2.13/cuda.html). |
| Accelerate hỗ trợ CPU/disk offload nhưng có overhead và giới hạn; `device_map="auto"` không tự chứng minh latency hay độ ổn định của từng model. | Offload chỉ bật theo model manifest sau benchmark, không hứa chung cho cả 49 voice/model. Xem [Accelerate Big Model Inference](https://huggingface.co/docs/accelerate/main/concept_guides/big_model_inference). |

## 2. Định nghĩa trạng thái và contract mục tiêu

### 2.1. Trạng thái worker

Coordinator và Desktop phải dùng cùng state machine:

```text
DISCONNECTED
  -> CONNECTING
  -> CONFIGURED        (URL/token đúng, inventory đọc được; chưa chiếm GPU)
  -> LEASING            (đang xin capability/model cụ thể)
  -> STARTING           (worker exact được tạo hoặc khởi động lại)
  -> READY              (health + capabilities + model/variant + CUDA pass)
  -> RUNNING
  -> RELEASING
  -> CONFIGURED

Mọi state -> FAILED -> DISCONNECTED hoặc CONFIGURED sau recovery có giới hạn.
```

`READY` chỉ có nghĩa stage đang giữ lease đã qua live health check. Không dùng
`ready=true` để ngụ ý mọi model trong inventory đều đang resident trên GPU.

### 2.2. Contract API mục tiêu

Giữ các route tương thích hiện tại:

```text
GET  /health
GET  /v1/capabilities
POST /v1/unified/lease
POST /v1/unified/lease/{lease_id}/release
ANY  /v1/unified/{capability}/{model}/{worker_route}
```

Tất cả route yêu cầu `Authorization: Bearer <token>`, trừ khi test local đã bật
explicit test flag. Lease request phải có `request_id`, `capability`, `model`; phản
hồi có `lease_id`, `expires_at`, `worker_revision`, `device`, `state`. Proxy không
được tự biến payload của một worker thành payload của worker khác.

`/health` phải trả tối thiểu `contract_version`, `coordinator_revision`,
`ready`, `device`, `active_lease_id` hoặc `null`, `workers`, `last_error_code`.
`/v1/capabilities` phải khai báo từng exact capability/model/variant, notebook
revision, route template, response contract, memory budget và trạng thái
`configured|starting|ready|unavailable`; field `loaded=true` chỉ xuất hiện khi
worker đã live và kiểm tra thành công.

### 2.3. Phạm vi remote/local

| Nhóm | Quyết định |
|---|---|
| Remote optional | separation, STT, subtitle OCR, translation, forced alignment, TTS/voice clone/design khi exact worker đã được kiểm chứng. |
| Local bắt buộc | media ingest, project setup, EBU R128 normalize, timeline/mix/sidechain, mux/export và file persistence. |
| Không mặc định | Tailscale auto-discovery, public unauthenticated endpoint, monolithic “8-task inference server”, silent fallback, prewarm 49 voices. |

## 3. Kế hoạch triển khai theo task

### Task 0 — Đóng băng contract và baseline

**Files:**

- `src/controllers/dubbing/DubbingColabModelRoutes.h`
- `src/controllers/dubbing/DubbingController.h`
- `src/controllers/dubbing/parts/DubbingController_Colab.cpp`
- `src/remote/colab/ColabSession.cpp`
- `notebooks/workers/LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py`
- `notebooks/pipelines/LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb`
- `tests/colab/`, `tests/dubbing/`

**Việc làm:**

1. Xuất danh sách exact route/model hiện tại thành bảng có capability, model,
   variant, worker route, response contract và notebook revision.
2. Đánh dấu rõ route đã verified và route chỉ là catalog. Không thêm capability
   chỉ vì nó có trong UI.
3. Chạy baseline source-contract, C++ unit test và QML lint trước khi sửa.
4. Ghi lại mọi thay đổi hiện có trong working tree; không xóa hoặc reset thay đổi
   của người dùng.

**Cổng nghiệm thu:** mỗi route được tra ngược từ QML → controller → session →
coordinator → exact worker; route không tra được bị loại khỏi manifest, không bị
đánh dấu Ready.

### Task 1 — Manifest có version và pin source

**Files:**

- Create: `notebooks/workers/unified_worker_manifest.json`
- Create: `notebooks/workers/validate_unified_worker_manifest.py`
- Modify: `notebooks/pipelines/LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb`
- Modify: `src/controllers/dubbing/DubbingColabModelRoutes.h`
- Test: `tests/colab/test_unified_worker_manifest.py`

**Việc làm:**

1. Tạo schema manifest version 1. Mỗi worker có `capability`, `model`, `variant`,
   `source_repo`, immutable `source_commit`, `worker_revision`, `health_route`,
   `capabilities_route`, `inference_route`, `response_contract`,
   `memory_budget_mb`, `max_input_seconds`, `max_concurrency`, `supports_lease`,
   `offload_mode` và `enabled`.
2. Pin repo/commit và dependency lock. Pipeline không clone branch mặc định rồi
   chạy mù; nếu commit thiếu hoặc hash không đúng thì dừng trước khi mở tunnel.
3. Validator kiểm duplicate key, route traversal, model/capability mismatch,
   budget âm, commit rỗng, response contract thiếu và worker không có health route.
4. Generator dùng manifest để sinh `UNIFIED_WORKERS`; không duy trì hai danh sách
   model độc lập bằng tay.

**Test bắt buộc:** valid manifest, duplicate route, commit mismatch, unknown
capability, exact route không nằm trong `DubbingColabModelRoutes` đều phải có kết
quả fail rõ ràng.

### Task 2 — Coordinator lifecycle và GPU lease an toàn

**Files:**

- Modify: `notebooks/workers/LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py`
- Create: `notebooks/workers/unified_worker_runtime.py`
- Test: `notebooks/workers/test_unified_worker_runtime.py`

**Việc làm:**

1. Tách `WorkerSpec`, `WorkerProcess`, `WorkerLease`, `GpuBudget` và
   `CoordinatorState` thành các lớp có state transition kiểm chứng được.
2. `SAFE_T4` chỉ cấp một GPU lease tại một thời điểm. Worker không thuộc stage
   hiện hành phải dừng process và chờ cleanup; không coi việc start tuần tự là
   đủ.
3. Trước khi load model, kiểm `torch.cuda.is_available()`, device name, free/total
   memory, manifest budget và input limit. Từ chối sớm nếu thiếu budget.
4. Sau request: đóng stream/file, hủy reference tensor/model theo adapter, gọi
   `torch.cuda.synchronize()` khi cần, `empty_cache()` best-effort, ghi metrics,
   rồi release/stop worker. Không ghi thông báo “VRAM đã giải phóng 100%”.
5. Dùng subprocess isolation để worker crash/OOM không kéo coordinator chết; sau
   crash chỉ retry một lần nếu `request_id` chưa tạo artifact hoàn tất. Retry phải
   có idempotency key.
6. Lease có TTL, heartbeat hoặc last-seen, owner/request id, max duration và
   cleanup trong `finally`. Lease hết hạn phải hủy request, stop worker và giải
   phóng lock.
7. Chỉ dùng `async def`/`httpx.AsyncClient` cho I/O; inference blocking chạy trong
   worker process, không chạy trên event loop của FastAPI. FastAPI cho phép trộn
   `async def` và `def` tùy thư viện blocking, theo [tài liệu chính thức](https://fastapi.tiangolo.com/async/).

**Cổng nghiệm thu:** hai request khác capability chạy đồng thời phải cho một
request `409/429` hoặc chờ lease, không được đồng thời load hai GPU model trong
profile `SAFE_T4`; worker crash được nhận diện và route chuyển `FAILED` có hướng
dẫn.

### Task 3 — Desktop session lazy activation, không báo Ready giả

**Files:**

- Modify: `src/remote/colab/ColabSession.h/.cpp`
- Modify: `src/controllers/dubbing/DubbingController.h`
- Modify: `src/controllers/dubbing/parts/DubbingController_Colab.cpp`
- Modify: `src/controllers/dubbing/parts/DubbingController_Workflow.cpp`
- Test: `tests/colab/`, `tests/dubbing/test_DubbingProject.cpp`

**Việc làm:**

1. Giữ `connectUnifiedWorkflowColab()` là bước validate URL/token và inventory;
   không bắt tất cả model phải `loaded=true` cùng lúc.
2. Thêm internal stage methods tương đương
   `acquireUnifiedStage(capability, model, requestId)` và
   `releaseUnifiedStage(leaseId)`. Chúng phải đi qua `ColabSession`, không để QML
   tự gọi HTTP.
3. Tách rõ `configured`, `verified`, `processing`, `stale`, `failed` trong state
   C++; QML chỉ hiển thị “Available” khi inventory đúng, “Ready” khi stage đang
   lease/live verified.
4. Khi Colab reset: xóa session verification cũ, giữ workflow node là blocked,
   giữ artifact đã commit, hiển thị CTA kết nối lại; không chạy tiếp với URL/token
   stale.
5. Khi user hủy hoặc đóng project, release lease và cancel request; nếu release
   thất bại vẫn xóa local lease state sau timeout để tránh UI bị kẹt.

**Cổng nghiệm thu:** kết nối unified thành công không cần prewarm toàn bộ model;
chạy từng stage tự acquire/release; model sai, variant sai, token sai, route sai,
worker reset và lease timeout đều không làm workflow chuyển Completed.

### Task 4 — Artifact contract và tính tiếp tục workflow

**Files:**

- Modify: `src/dubbing/project/DubbingProject.cpp`
- Modify: `src/controllers/dubbing/parts/DubbingController_Workflow.cpp`
- Modify: exact remote runners dưới `src/*/runners/`
- Test: `tests/dubbing/test_DubbingProject.cpp`, `tests/colab/`

**Việc làm:**

1. Mỗi stage ghi artifact vào file tạm có fingerprint nguồn, capability, model,
   worker revision, request id và checksum; chỉ rename atomic sang output cuối sau
   khi validate schema/media.
2. Không coi HTTP 200 là thành công nếu body thiếu file, duration, sample rate,
   segments, language hoặc response contract tương ứng.
3. Resume chỉ dùng artifact đã hoàn tất và fingerprint khớp; artifact dở dang bị
   quarantine, không bị dùng như stem/voice/subtitle hợp lệ.
4. Thiếu vocals/background, transcript rỗng, source audio mất hoặc output không
   đọc được phải dừng tại node hiện tại và đưa CTA cụ thể; không thay stem bằng
   normalized source một cách im lặng.

**Cổng nghiệm thu:** kill coordinator giữa mỗi stage rồi restart; workflow không
   mất artifact đã commit, không tạo output trùng, và không chạy node kế tiếp với
   dữ liệu thiếu.

### Task 5 — Tunnel/auth và bảo mật vận hành

**Files:**

- Modify: `notebooks/workers/LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py`
- Modify: `qml/components/dubbing/DubbingColabSetupDialog.qml`
- Modify: `src/remote/colab/ColabSession.cpp`
- Test: `tests/colab/`, QML source-contract tests

**Việc làm:**

1. Token sinh ngẫu nhiên đủ entropy, chỉ in token khi user chủ động yêu cầu; log
   không được ghi bearer token, payload audio hoặc đường dẫn có secret.
2. Desktop bắt buộc HTTPS cho endpoint public; chỉ cho HTTP loopback ở test.
3. URL/token lưu local chỉ khi user chọn “remember”; dùng secure credential store,
   không ghi plaintext vào project JSON.
4. Quick Tunnel là development preview. Production mode phải yêu cầu managed
   tunnel/endpoint do user kiểm soát, hoặc hiển thị cảnh báo trước khi kết nối.
5. Kiểm status code: 401/403 auth, 404 capability/route, 409 lease conflict,
   429 rate/concurrency, 502 tunnel upstream, 503 worker unavailable, timeout và
   Colab reset phải có mã lỗi ổn định để `AppErrorCatalog` đưa ra hướng dẫn.

**Cổng nghiệm thu:** quét log không còn token; token sai không làm lộ inventory
   nhạy cảm; endpoint HTTP public bị từ chối; route bị sửa path hoặc capability
   không có trong manifest bị reject trước inference.

### Task 6 — Notebook bootstrap reproducible

**Files:**

- Modify: `notebooks/pipelines/LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb`
- Modify: `notebooks/workers/LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py`
- Create: `notebooks/workers/requirements-unified-colab.txt`
- Create: `notebooks/workers/colab_preflight.py`

**Việc làm:**

1. Step 0 kiểm Python, CUDA, GPU name, VRAM, disk, network và package versions;
   output thành JSON preflight để Desktop/QA lưu lại.
2. Cài dependencies có pin/constraint và chạy import smoke test trước khi start
   bất kỳ worker nào.
3. Chỉ bật worker có artifact/model file đủ; worker thiếu model trả `unavailable`
   với `missing_dependency`/`missing_model`, không khởi động nửa vời.
4. Dùng local `/content` cho scratch/cache lớn; copy batch/archive thay vì nhiều
   I/O nhỏ trên Drive. Artifact cuối đồng bộ có checksum.
5. Giữ bootstrap idempotent: chạy lại cell không nhân đôi process, port, tunnel,
   hoặc file cấu hình.

**Cổng nghiệm thu:** runtime sạch có thể preflight fail đúng lý do; runtime đã
   chạy có thể re-run bootstrap mà không tạo coordinator thứ hai; phiên reset có
   thể tạo endpoint mới và Desktop reconnect sau một lần nhập lại.

### Task 7 — UI/UX trạng thái và hướng dẫn xử lý

**Files:**

- Modify: `qml/components/dubbing/DubbingColabSetupDialog.qml`
- Modify: `qml/components/dubbing/DubbingProjectStatusPanel.qml`
- Modify: `qml/components/shared/ErrorGuidanceDialog.qml`
- Modify: `qml/components/shared/ErrorGuidanceInline.qml`
- Modify: `src/controllers/app/AppErrorCatalog.cpp`
- Modify: `recheck.md` và 16 tài liệu audit khi contract đổi

**Việc làm:**

1. Hiển thị ba lớp trạng thái: `Available` (inventory), `Ready` (live lease),
   `Running` (request). Không dùng một chữ “Ready” cho cả coordinator và mọi
   stage.
2. Với lỗi, giữ technical detail trong log/accordion; trên màn hình có title,
   nguyên nhân, bước xử lý, CTA `Reconnect`, `Open Colab`, `Retry`, `Use Local`
   hoặc `Choose another model` tùy lỗi.
3. Cảnh báo rõ khi dùng Quick Tunnel preview; không gọi là production hoặc
   zero-config. Token được che và không đi vào clipboard/report mặc định.
4. Disable Run khi stage chưa `Ready`, disable Continue khi artifact chưa hợp lệ;
   nút Retry phải idempotent và không tạo request song song.
5. Kiểm responsive ở 1280×720, 1920×1080, 2560×1440 và 3840×2160; URL/model dài
   dùng `elide: Text.ElideMiddle`, guidance dùng `Text.Wrap`, panel dài dùng
   `ScrollView/Flickable`.

**Cổng nghiệm thu:** user nhìn thấy cách xử lý trong một màn hình; không bị popup
   chồng, không tràn chữ, không có nút Run xanh khi thiếu worker/stem/lease.

### Task 8 — Test matrix và fault injection

**Files:**

- Create: `tests/colab/test_unified_coordinator_contract.py`
- Create: `tests/colab/test_unified_coordinator_faults.py`
- Modify: `tests/CMakeLists.txt`, `tests/dubbing/test_DubbingProject.cpp`
- Modify: `PRE_DELIVERY_CHECKLIST.md`

**Test matrix tối thiểu:**

| Nhóm | Ca kiểm thử |
|---|---|
| Contract | valid health/capabilities; exact model; wrong variant; stale revision; missing route; malformed JSON; response schema mismatch. |
| Auth/network | missing token; wrong token; HTTPS policy; timeout; DNS/tunnel down; 401/403/404/409/429/502/503. |
| Lifecycle | worker startup fail; worker crash; coordinator crash; Colab reset; lease expiry; cancel during upload/inference/download. |
| VRAM | insufficient free memory; repeated 30 jobs; sequential model switch; process stop; peak memory and cleanup metric. |
| Workflow | missing stem; empty transcript; partial artifact; source fingerprint mismatch; resume after restart; no silent fallback. |
| UI | long URL; narrow window; modal dismiss; keyboard focus; screen-reader labels; ErrorGuidance CTA; no token exposure. |
| Real runtime | at least one clean Colab GPU runtime for each enabled exact worker; record GPU name, CUDA/Python/package versions and measured timings. |

**Cổng nghiệm thu:** test fault nào không thể tái hiện phải ghi `NOT TESTED` và lý
   do; không được đổi thành PASS chỉ vì unit test không bao phủ runtime.

### Task 9 — Observability, support bundle và rollback

**Files:**

- Modify: `src/core/utils/Logger.cpp`
- Modify: `src/remote/colab/ColabSession.cpp`
- Modify: `notebooks/workers/LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py`
- Modify: `qml/components/shared/ErrorGuidanceDialog.qml`

**Việc làm:**

1. Mọi request có correlation id; log state transition, worker revision, duration,
   status code, retry count, peak memory và artifact checksum, nhưng redact token,
   audio content và PII.
2. `createProblemReport()` chỉ thu diagnostics đã redact, endpoint host và revision;
   không thu bearer token hoặc raw transcript nếu user chưa chọn.
3. Feature flag `unified_colab_safe_t4` mặc định off cho rollout đầu, bật theo
   allowlist; nếu disable thì route individual hiện tại vẫn hoạt động.
4. Rollback chỉ cần tắt flag/manifest entry và dùng route exact cũ; không migration
   phá project artifact.

### Task 10 — Rollout có đo lường

**Giai đoạn:**

1. `alpha-local`: coordinator loopback, không tunnel, fake worker và fault tests.
2. `alpha-colab`: một user, một capability, one-time URL/token, Quick Tunnel có
   banner preview.
3. `beta-safe-t4`: nhiều capability nhưng chỉ một active GPU lease; 30 job lặp,
   restart và reset test.
4. `release-candidate`: managed endpoint/runtime do user kiểm soát; không yêu cầu
   Quick Tunnel để chứng minh production reliability.

## 4. Tiêu chí đạt 10/10

Chỉ chấm 10/10 khi tất cả điều kiện sau có bằng chứng:

- 100% enabled route có manifest, source revision, exact model/variant và response
  contract được kiểm thử.
- Không có route public unauthenticated; token không xuất hiện trong log/report.
- `SAFE_T4` không load đồng thời hai GPU worker; mọi request đều thuộc lease có
  TTL và correlation id.
- 30 job liên tiếp cho mỗi worker enabled trên runtime được chọn không có artifact
  sai, race, output chồng hoặc OOM chưa xử lý; peak memory được ghi lại. Nếu Colab
  cấp GPU khác, phải tạo profile/budget mới, không suy diễn từ T4.
- Worker crash, Colab reset, tunnel mất, token sai, model thiếu và response sai
  đều đưa UI tới hướng dẫn cụ thể; workflow không chuyển Completed và không silent
  fallback.
- C++ focused tests, Python coordinator tests, QML lint, source-contract tests và
  manual Colab matrix đều PASS; ca chưa chạy phải được ghi rõ.
- Local workflow 8 task vẫn chạy được khi không có Colab; remote chỉ thay đúng các
  stage được cấu hình.
- Rollback feature flag thành công và các artifact dự án cũ vẫn mở được.

## 5. Các tuyên bố bị cấm trong tài liệu/release note

Không dùng các câu sau nếu chưa có SLA/hạ tầng riêng chứng minh bằng văn bản:

- “Colab luôn có GPU/T4 15 GB.”
- “Không bao giờ OOM.”
- “100% miễn phí.”
- “Khởi động chắc chắn trong 1,5–2 phút.”
- “Zero-config không cần URL/token.”
- “Một FastAPI server chứa toàn bộ 8 task.”
- “`torch.cuda.empty_cache()` giải phóng toàn bộ VRAM.”

## 6. Trình tự thực thi và điều kiện dừng

Thực thi theo thứ tự Task 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10. Mỗi task
phải có test trước, implementation nhỏ, test xanh và review diff. Dừng rollout nếu
phát hiện một trong các điều kiện: token lộ log; route không exact; worker không
được lease nhưng vẫn nhận inference; artifact chưa validate; UI hiển thị Ready giả;
hoặc fault test biến mất do test bị skip.

Không build/package EXE trong phạm vi kế hoạch này. Khi implementation hoàn tất,
chỉ build release sau khi người dùng yêu cầu riêng và checklist pre-delivery đã
được cập nhật theo kết quả thực tế.

## 7. Kết luận khả thi

Phương án đạt mức **10/10 về tính khả thi của kế hoạch** nếu triển khai đúng các
task và cổng nghiệm thu trên: một URL/token, exact worker, lazy GPU lease, process
isolation, artifact validation, UI guidance và rollback. Phương án **không** đạt
10/10 nếu vẫn giữ prewarm tất cả model, Quick Tunnel như production, Tailscale
hostname tự đoán, fallback im lặng hoặc các cam kết về quota/VRAM/thời gian mà
Colab không bảo đảm.
