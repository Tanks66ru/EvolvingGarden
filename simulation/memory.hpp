#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace evolvinggarden {
enum class EventType : std::uint8_t {
    Birth, Fed, Petted, NoveltyDiscovered, BroughtToForeground,
    Startled, BondMilestone, FirstSocialContact, HabitFormed, Count
};
struct MemoryEvent {
    std::uint32_t timestampSeconds = 0;
    EventType type = EventType::Birth;
    std::int8_t emotionalImpact = 0;
    std::uint8_t significance = 0;
    std::uint16_t relatedCreatureId = 0;
};
class MemoryJournal {
public:
    static constexpr std::size_t Capacity = 24;
    static constexpr std::uint8_t MinimumSignificance = 75;
    bool add(const MemoryEvent& event) {
        if (event.significance < MinimumSignificance) return false;
        if (size_ == Capacity) {
            for (std::size_t i = 1; i < Capacity; ++i) events_[i - 1] = events_[i];
            --size_;
        }
        events_[size_++] = event;
        return true;
    }
    std::size_t size() const { return size_; }
    const MemoryEvent& at(std::size_t index) const { return events_[index]; }
    const std::array<MemoryEvent, Capacity>& entries() const { return events_; }
private:
    std::array<MemoryEvent, Capacity> events_{};
    std::size_t size_ = 0;
};
inline std::string_view eventTitle(EventType type) {
    switch (type) {
    case EventType::Birth: return "A new life begins";
    case EventType::Fed: return "A meal after severe hunger";
    case EventType::Petted: return "An important moment of contact";
    case EventType::NoveltyDiscovered: return "An unusual discovery";
    case EventType::BroughtToForeground: return "Chosen for the main screen";
    case EventType::Startled: return "A frightening experience";
    case EventType::BondMilestone: return "A strong bond was formed";
    case EventType::FirstSocialContact: return "First social contact";
    case EventType::HabitFormed: return "A new habit formed";
    case EventType::Count: break;
    }
    return "Unknown event";
}
inline std::string_view eventDescription(EventType type) {
    switch (type) {
    case EventType::Birth: return "The creature entered the world.";
    case EventType::Fed: return "The creature was fed after becoming extremely hungry.";
    case EventType::Petted: return "A rare interaction had a strong emotional effect.";
    case EventType::NoveltyDiscovered: return "An unusual experience left a lasting impression.";
    case EventType::BroughtToForeground: return "The creature was selected for individual attention.";
    case EventType::Startled: return "A sudden stimulus caused an unusually strong fear response.";
    case EventType::BondMilestone: return "Its relationship with the player reached a major milestone.";
    case EventType::FirstSocialContact: return "The creature interacted with another inhabitant.";
    case EventType::HabitFormed: return "A repeated experience became a lasting preference.";
    case EventType::Count: break;
    }
    return "";
}
}
