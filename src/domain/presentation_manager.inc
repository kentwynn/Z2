uint8_t presentationPriority(PresentationOwner owner)
{
    switch (owner) {
    case PresentationOwner::Safety: return 6;
    case PresentationOwner::System: return 5;
    case PresentationOwner::Voice: return 4;
    case PresentationOwner::Interaction: return 3;
    case PresentationOwner::Control: return 2;
    case PresentationOwner::Identity: return 1;
    default: return 0;
    }
}

FaceEmotion presentationEmotion(String value)
{
    value.toUpperCase();
    if (value == "HAPPY") return FaceEmotion::Happy;
    if (value == "CURIOUS") return FaceEmotion::Curious;
    if (value == "SLEEPY") return FaceEmotion::Sleepy;
    if (value == "SURPRISED") return FaceEmotion::Surprised;
    if (value == "EXCITED") return FaceEmotion::Excited;
    if (value == "SAD") return FaceEmotion::Sad;
    if (value == "LOVE") return FaceEmotion::Love;
    if (value == "CONFUSED") return FaceEmotion::Confused;
    if (value == "THINKING") return FaceEmotion::Thinking;
    if (value == "ANGRY") return FaceEmotion::Angry;
    if (value == "ALERT") return FaceEmotion::Alert;
    if (value == "FOCUSED") return FaceEmotion::Focused;
    if (value == "FURIOUS") return FaceEmotion::Furious;
    if (value == "SCARED" || value == "AFRAID") return FaceEmotion::Scared;
    if (value == "WORRIED" || value == "ANXIOUS") return FaceEmotion::Worried;
    if (value == "BORED") return FaceEmotion::Bored;
    if (value == "DESPAIR") return FaceEmotion::Despair;
    if (value == "CALM" || value == "CONTENT") return FaceEmotion::Neutral;
    return FaceEmotion::Neutral;
}

void applyPresentation(const PresentationSlot &slot, bool playEntrySound)
{
    gFaceAnimation = slot.preset.animation.length() ? slot.preset.animation : "STATIC";
    String oled = slot.preset.oled; oled.toUpperCase();
    if (oled == "OFF") gDisplayMode = DisplayMode::Off;
    else if (oled == "INFO") gDisplayMode = DisplayMode::Info;
    else if (oled == "SENSORS") gDisplayMode = DisplayMode::Sensors;
    else { gDisplayMode = DisplayMode::Face; gFaceEmotion = presentationEmotion(oled); }
    gLastOledUpdateMs = 0;
    if (playEntrySound && gCapabilitySpeaker && slot.preset.sound != "OFF") playSound(slot.preset.sound);
}

void refreshPresentation(PresentationOwner changedOwner, bool playEntrySound)
{
    PresentationOwner selected = PresentationOwner::Idle;
    for (uint8_t i = 0; i < static_cast<uint8_t>(PresentationOwner::Count); ++i) {
        auto owner = static_cast<PresentationOwner>(i);
        if (gPresentationSlots[i].active && presentationPriority(owner) >= presentationPriority(selected)) selected = owner;
    }
    applyPresentation(gPresentationSlots[static_cast<uint8_t>(selected)], playEntrySound && selected == changedOwner);
}

void requestPresentation(PresentationOwner owner, const PresentationPreset &preset, const char *event, bool playEntrySound)
{
    if (owner == PresentationOwner::Safety)
        interruptRealtimePlaybackForSafety();
    if (owner == PresentationOwner::Control || owner == PresentationOwner::Interaction ||
        owner == PresentationOwner::Safety) {
        // A live physical reaction must be visible immediately. Temporary AI
        // toolbox text must not cover the eyes while Z2 moves, is touched, or
        // responds to a safety event.
        gOledConsoleUntilMs = 0;
        gOledLiveDetail = "";
    }
    auto &slot = gPresentationSlots[static_cast<uint8_t>(owner)];
    const bool changed = !slot.active || slot.event != event;
    slot.active = true; slot.preset = preset; slot.event = event;
    refreshPresentation(owner, playEntrySound && changed);
    if (changed) {
        Serial.printf("[Z2 PRESENTATION] owner=%u event=%s oled=%s animation=%s sound=%s\n", static_cast<unsigned>(owner), event, preset.oled.c_str(), preset.animation.c_str(), preset.sound.c_str());
        const char *tools[] = {"Idle", "Memory & Identity", "Voice Chat", "Control", "Interaction", "System", "Safety"};
        sendDeviceEvent(tools[static_cast<uint8_t>(owner)], event,
                        preset.oled + " · " + preset.animation + " · " + preset.sound);
    }
}

void clearPresentation(PresentationOwner owner)
{
    gPresentationSlots[static_cast<uint8_t>(owner)].active = false;
    refreshPresentation(owner, false);
}
