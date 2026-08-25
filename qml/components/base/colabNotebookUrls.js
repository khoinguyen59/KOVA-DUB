.pragma library

// Keep the repository and release branch used by every "Open in Colab" action
// in one audited place. Controllers decide the exact notebook filename from
// the selected model; this helper only turns that filename into a URL.  Do not
// point this at a development branch: a model selection must open the notebook
// that is committed on the repository's main release branch.
var notebookBaseUrl = "https://colab.research.google.com/github/khoinguyen59/kova-video-studio/blob/main/notebooks/"

var notebookSubpaths = {
    // TTS
    "LA_STUDIO_TTS_KOKORO_GPU.ipynb": "tts/LA_STUDIO_TTS_KOKORO_GPU.ipynb",
    "LA_STUDIO_TTS_KOKORO_VIETNAMESE_GPU.ipynb": "tts/LA_STUDIO_TTS_KOKORO_VIETNAMESE_GPU.ipynb",
    "LA_STUDIO_TTS_QWEN3_CUSTOMVOICE_1_7B_GPU.ipynb": "tts/LA_STUDIO_TTS_QWEN3_CUSTOMVOICE_1_7B_GPU.ipynb",
    "LA_STUDIO_TTS_VIBEVOICE_0_5B_GPU.ipynb": "tts/LA_STUDIO_TTS_VIBEVOICE_0_5B_GPU.ipynb",
    "LA_STUDIO_TTS_VIENEU_V2_TURBO_GPU.ipynb": "tts/LA_STUDIO_TTS_VIENEU_V2_TURBO_GPU.ipynb",
    "LA_STUDIO_TTS_VIENEU_V3_TURBO_GPU.ipynb": "tts/LA_STUDIO_TTS_VIENEU_V3_TURBO_GPU.ipynb",
    "LA_STUDIO_TTS_VOXCPM2_GPU.ipynb": "tts/LA_STUDIO_TTS_VOXCPM2_GPU.ipynb",
    // STT
    "LA_STUDIO_STT_NEMOTRON_3_5_0_6B_GPU.ipynb": "stt/LA_STUDIO_STT_NEMOTRON_3_5_0_6B_GPU.ipynb",
    "LA_STUDIO_STT_QWEN3_ASR_0_6B_GPU.ipynb": "stt/LA_STUDIO_STT_QWEN3_ASR_0_6B_GPU.ipynb",
    "LA_STUDIO_STT_QWEN3_ASR_1_7B_GPU.ipynb": "stt/LA_STUDIO_STT_QWEN3_ASR_1_7B_GPU.ipynb",
    "LA_STUDIO_STT_WHISPER_GPU.ipynb": "stt/LA_STUDIO_STT_WHISPER_GPU.ipynb",
    // Voice Cloning
    "LA_STUDIO_TTS_OMNIVOICE_GPU.ipynb": "voice_cloning/LA_STUDIO_TTS_OMNIVOICE_GPU.ipynb",
    "LA_STUDIO_VOICE_CLONE_GPU.ipynb": "voice_cloning/LA_STUDIO_VOICE_CLONE_GPU.ipynb",
    "LA_STUDIO_VOICE_CLONE_OMNIVOICE_GPU.ipynb": "voice_cloning/LA_STUDIO_VOICE_CLONE_OMNIVOICE_GPU.ipynb",
    "LA_STUDIO_VOICE_CLONE_QWEN3_BASE_0_6B_GPU.ipynb": "voice_cloning/LA_STUDIO_VOICE_CLONE_QWEN3_BASE_0_6B_GPU.ipynb",
    "LA_STUDIO_VOICE_CLONE_QWEN3_BASE_1_7B_GPU.ipynb": "voice_cloning/LA_STUDIO_VOICE_CLONE_QWEN3_BASE_1_7B_GPU.ipynb",
    "LA_STUDIO_VOICE_CLONE_VIENEU_V2_TURBO_GPU.ipynb": "voice_cloning/LA_STUDIO_VOICE_CLONE_VIENEU_V2_TURBO_GPU.ipynb",
    "LA_STUDIO_VOICE_CLONE_VIENEU_V3_TURBO_GPU.ipynb": "voice_cloning/LA_STUDIO_VOICE_CLONE_VIENEU_V3_TURBO_GPU.ipynb",
    "LA_STUDIO_VOICE_CLONE_VOXCPM2_GPU.ipynb": "voice_cloning/LA_STUDIO_VOICE_CLONE_VOXCPM2_GPU.ipynb",
    // Voice Design
    "LA_STUDIO_VOICE_DESIGN_GPU.ipynb": "voice_design/LA_STUDIO_VOICE_DESIGN_GPU.ipynb",
    "LA_STUDIO_VOICE_DESIGN_OMNIVOICE_GPU.ipynb": "voice_design/LA_STUDIO_VOICE_DESIGN_OMNIVOICE_GPU.ipynb",
    "LA_STUDIO_VOICE_DESIGN_QWEN3_1_7B_GPU.ipynb": "voice_design/LA_STUDIO_VOICE_DESIGN_QWEN3_1_7B_GPU.ipynb",
    "LA_STUDIO_VOICE_DESIGN_VOXCPM2_GPU.ipynb": "voice_design/LA_STUDIO_VOICE_DESIGN_VOXCPM2_GPU.ipynb",
    // Voice Separation
    "LA_STUDIO_SEPARATION_GPU.ipynb": "voice_separation/LA_STUDIO_SEPARATION_GPU.ipynb",
    "LA_STUDIO_SEPARATION_SPLEETER_2STEMS_GPU.ipynb": "voice_separation/LA_STUDIO_SEPARATION_SPLEETER_2STEMS_GPU.ipynb",
    "LA_STUDIO_SEPARATION_UVR_VOCALS_GPU.ipynb": "voice_separation/LA_STUDIO_SEPARATION_UVR_VOCALS_GPU.ipynb",
    // Subtitle OCR
    "LA_STUDIO_SUBTITLE_OCR_PP_OCRV5_GPU.ipynb": "subtitle_ocr/LA_STUDIO_SUBTITLE_OCR_PP_OCRV5_GPU.ipynb",
    // Alignment
    "LA_STUDIO_ALIGNMENT_CANARY_CTC_GPU.ipynb": "alignment/LA_STUDIO_ALIGNMENT_CANARY_CTC_GPU.ipynb",
    "LA_STUDIO_ALIGNMENT_GPU.ipynb": "alignment/LA_STUDIO_ALIGNMENT_GPU.ipynb",
    "LA_STUDIO_ALIGNMENT_MMS_ONNX_GPU.ipynb": "alignment/LA_STUDIO_ALIGNMENT_MMS_ONNX_GPU.ipynb",
    "LA_STUDIO_ALIGNMENT_QWEN3_0_6B_GPU.ipynb": "alignment/LA_STUDIO_ALIGNMENT_QWEN3_0_6B_GPU.ipynb",
    "LA_STUDIO_ALIGNMENT_WAV2VEC2_ZH_GPU.ipynb": "alignment/LA_STUDIO_ALIGNMENT_WAV2VEC2_ZH_GPU.ipynb",
    // Translation
    "LA_STUDIO_TRANSLATION_HY_MT2_1_8B_GPU.ipynb": "translation/LA_STUDIO_TRANSLATION_HY_MT2_1_8B_GPU.ipynb",
    "LA_STUDIO_TRANSLATION_M2M100_418M_GPU.ipynb": "translation/LA_STUDIO_TRANSLATION_M2M100_418M_GPU.ipynb",
    "LA_STUDIO_TRANSLATION_MADLAD400_3B_GPU.ipynb": "translation/LA_STUDIO_TRANSLATION_MADLAD400_3B_GPU.ipynb",
    // LLM
    "LA_STUDIO_LLM_QWEN3_5_2B_GPU.ipynb": "llm/LA_STUDIO_LLM_QWEN3_5_2B_GPU.ipynb",
    // Pipelines
    "LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb": "pipelines/LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb",
    "LA_STUDIO_SPEECH_GPU.ipynb": "pipelines/LA_STUDIO_SPEECH_GPU.ipynb",
    "LA_STUDIO_VOICE_GPU.ipynb": "pipelines/LA_STUDIO_VOICE_GPU.ipynb",
    "LA_STUDIO_LANGUAGE_GPU.ipynb": "pipelines/LA_STUDIO_LANGUAGE_GPU.ipynb"
}

function forNotebookFile(fileName) {
    if (!fileName) return ""
    var subpath = notebookSubpaths[fileName] || fileName
    return notebookBaseUrl + subpath
}
