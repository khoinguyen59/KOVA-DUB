#!/usr/bin/env python3
"""
Generate Reference Audio WAV files for all VieNeu-TTS Voice Presets.
==================================================================
This script synthesizes 4-6s reference WAV audio clips for all 20 VieNeu-TTS
Turbo presets (including Central, Southern, and Northern dialect voices like
Quang Sơn, Ngọc Trân, Thái Sơn, Mai Anh, etc.) so that they can be used directly
by OmniVoice, F5-TTS, and VoiceStudio for zero-shot cross-lingual voice cloning.

Usage:
    python scripts/render_vieneu_cloning_references.py [--device cuda|cpu]
"""

import os
import sys
import json
from pathlib import Path

# Paths
REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_PRESETS_DIR = REPO_ROOT / "data" / "presets"
REFS_DIR = DATA_PRESETS_DIR / "voice_clone_refs"
VIENEU_REPO = REPO_ROOT.parent / "VieNeu-TTS"

# Sample prompt sentences optimized for acoustic richness and pitch clarity
STANDARD_PROMPTS = {
    "tin_tuc": "Trên thực tế, các chuyên gia kinh tế đã dự báo sự tăng trưởng mạnh mẽ của thị trường trong giai đoạn tới.",
    "doc_truyen": "Đêm dần về khuya, không gian trở nên tĩnh lặng, chỉ còn lại tiếng gió khẽ lay nhẹ qua những tán cây ngoài hiên.",
    "tu_nhien": "Chào bạn, hôm nay thời tiết rất trong lành và mát mẻ, chúc bạn một ngày làm việc tràn đầy năng lượng và niềm vui.",
    "le_hoi": "Tết là dịp mọi người háo hức đón chào một năm mới với nhiều hy vọng, may mắn và hạnh phúc sum vầy.",
    "khoa_hoc": "Ví dụ hai, tính giá trị trung bình và phân tích phương sai của dãy dữ liệu với độ chính xác cao.",
    "dam_thoai": "Hôm nay trời đẹp ghê, tụi mình cùng ghé quán cà phê quen ngồi tán gẫu một chút nha."
}

def get_voice_filename(name: str, gender: str, region: str) -> str:
    slug = name.lower().replace(" ", "_")
    g = "nam" if gender == "male" else "nu"
    r = region.lower().replace(" ", "_")
    return f"vn_{slug}_{g}_{r}.wav"

def main():
    REFS_DIR.mkdir(parents=True, exist_ok=True)
    turbo_json = DATA_PRESETS_DIR / "vieneu_voices_v3_turbo.json"
    
    if not turbo_json.exists():
        print(f"Error: {turbo_json} not found.")
        sys.exit(1)
        
    with open(turbo_json, "r", encoding="utf-8") as f:
        data = json.load(f)
        
    presets = data.get("presets", {})
    print(f"Found {len(presets)} VieNeu Turbo presets to check/generate.")
    
    # Check if VieNeu is importable locally
    vieneu_available = False
    tts = None
    try:
        sys.path.insert(0, str(VIENEU_REPO / "src"))
        from vieneu import Vieneu
        print("VieNeu package detected. Initializing engine for reference rendering...")
        tts = Vieneu(mode="v3turbo")
        vieneu_available = True
    except Exception as e:
        print(f"Note: Local VieNeu engine not loaded directly ({e}).")
        print("This script can be executed on Google Colab or local Python environment with VieNeu installed.")

    catalog_updates = []
    
    for name, p in presets.items():
        gender = p.get("gender", "male")
        region = p.get("region", "Bắc")
        style = p.get("style", "tu_nhien")
        target_wav_name = get_voice_filename(name, gender, region)
        target_wav_path = REFS_DIR / target_wav_name
        prompt_text = STANDARD_PROMPTS.get(style, STANDARD_PROMPTS["tu_nhien"])
        
        status = "EXISTS" if target_wav_path.exists() else "PENDING_RENDER"
        
        if not target_wav_path.exists() and vieneu_available and tts:
            print(f"Rendering reference audio for '{name}' ({gender}, {region})...")
            try:
                wav = tts.infer(prompt_text, voice=name)
                tts.save(wav, str(target_wav_path))
                status = "RENDERED"
            except Exception as ex:
                print(f"Failed rendering {name}: {ex}")
                status = "FAILED"
                
        print(f"  - [{status:14}] Voice: {name:12} -> {target_wav_name}")
        catalog_updates.append({
            "name": name,
            "filename": target_wav_name,
            "transcript": prompt_text,
            "gender": gender,
            "region": region,
            "style": style
        })
        
    print("\nSummary:")
    print(f"Total presets cataloged: {len(catalog_updates)}")
    print(f"Reference directory: {REFS_DIR}")

if __name__ == "__main__":
    main()
