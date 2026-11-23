#pragma once
#include <cstddef>
#include <cstdint>
namespace evolvinggarden {
struct CreatureNameEntry { const char* title; const char* adjective; const char* noun; };
// Three editable columns: optional title, adjective, and noun.
inline constexpr CreatureNameEntry CreatureNameTable[] = {
 {"Grand","Furious","Chariot"},{"Emperor","Yellow","Hawkeye"},{"God","Crimson","Doom"},
 {"Pretty","Silent","Star"},{"Ancient","Velvet","Comet"},{"Royal","Restless","Oracle"},
 {"Prime","Golden","Ranger"},{"Elder","Midnight","Mantis"},{"Supreme","Electric","Mammoth"},
 {"Noble","Azure","Warden"},{"Savage","Cosmic","Phoenix"},{"Divine","Frosted","Viper"},
 {"Tiny","Thunderous","Golem"},{"Mighty","Crystalline","Specter"},{"Lucky","Scarlet","Raven"},
 {"Wild","Gentle","Titan"},{"Glorious","Shattered","Mirage"},{"Mystic","Radiant","Sphinx"},
 {"Perfect","Obsidian","Wolf"},{"Fearless","Lunar","Dragon"},{"Dread","Solar","Beetle"},
 {"Bright","Cunning","Nomad"},{"Magnificent","Restless","Monarch"},{"Fabled","Amber","Sentinel"},
 {"Blessed","Stormy","Jaguar"},{"Mighty","Quiet","Atlas"},{"Grand","Iron","Colossus"},
 {"Emperor","Emerald","Prowler"},{"God","Howling","Tempest"},{"Pretty","Silver","Dancer"},
 {"Ancient","Distant","Voyager"},{"Royal","Burning","Cobra"},{"Prime","Frozen","Glacier"},
 {"Elder","Wicked","Jester"},{"Supreme","Dreaming","Phantom"},{"Noble","Brave","Falcon"},
 {"Savage","Grinning","Goblin"},{"Divine","Shy","Whisper"},{"Tiny","Dazzling","Firefly"},
 {"Mighty","Grim","Reaper"},{"Lucky","Mellow","Badger"},{"Wild","Restless","Meteor"},
 {"Glorious","Violet","Knight"},{"Mystic","Hidden","Panda"},{"Perfect","Rapid","Arrow"},
 {"Fearless","Mossy","Giant"},{"Dread","Crackling","Spark"},{"Bright","Sleepy","Bear"},
 {"Magnificent","Endless","Horizon"},{"Fabled","Copper","Fox"},{"Blessed","Feral","Stag"},
 {"Mighty","Opal","Crown"},{"Grand","Gravitic","Comet"},{"Emperor","Scarred","Warlock"},
 {"God","Blazing","Sun"},{"Pretty","Clouded","Dream"},{"Ancient","Mercurial","Trickster"},
 {"Royal","Stoic","Guardian"},{"Prime","Wild","Howler"},{"Elder","Prismatic","Jewel"},
 {"Supreme","Fierce","Leviathan"},{"Noble","Wandering","Pilgrim"},{"Savage","Hollow","Echo"},
 {"Divine","Daring","Rocket"},{"Lucky","Thunder","Herald"}
};
inline constexpr std::size_t CreatureNameTableSize =
 sizeof(CreatureNameTable) / sizeof(CreatureNameTable[0]);
struct GeneratedCreatureName { const char* title; const char* adjective; const char* noun; };
constexpr std::uint32_t mixNameSeed(std::uint32_t v) {
 v ^= v >> 16U; v *= 0x7FEB352DU; v ^= v >> 15U;
 v *= 0x846CA68BU; v ^= v >> 16U; return v;
}
inline GeneratedCreatureName generateCreatureName(std::uint32_t seed) {
 const auto& title = CreatureNameTable[mixNameSeed(seed ^ 0xA511E9B3U) % CreatureNameTableSize];
 const auto& adjective = CreatureNameTable[mixNameSeed(seed ^ 0x63D83595U) % CreatureNameTableSize];
 const auto& noun = CreatureNameTable[mixNameSeed(seed ^ 0xB5297A4DU) % CreatureNameTableSize];
 const bool titled = (mixNameSeed(seed ^ 0x9E3779B9U) % 100U) < 5U;
 return {titled ? title.title : nullptr, adjective.adjective, noun.noun};
}
} // namespace evolvinggarden
