#include "creatures/genome.hpp"
#include "simulation/creature_simulation.hpp"
#include "simulation/memory.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
using namespace evolvinggarden;

static void advance(CreatureSimulation& creature, float seconds) {
    const int steps = static_cast<int>(seconds / 0.25F);
    for (int i = 0; i < steps; ++i) creature.update(0.25F);
}

int main() {
    {
        Genome genome = makeDefaultGenome();
        genome.personality.friendliness = 250;
        genome.personality.curiosity = -250;
        genome.morphology.appendageCount = 255;
        genome.normalize();
        assert(genome.personality.friendliness == 100);
        assert(genome.personality.curiosity == -100);
        assert(genome.morphology.appendageCount == 6);
        assert(genome.morphology.eyeSize == 62);
    }
    {
        MemoryJournal journal;
        assert(!journal.add(MemoryEvent{0, EventType::Fed, 0, 74, 0}));
        for (std::uint32_t i = 0; i < MemoryJournal::Capacity + 5; ++i)
            assert(journal.add(MemoryEvent{i, EventType::NoveltyDiscovered, 20, 85, 0}));
        assert(journal.size() == MemoryJournal::Capacity);
        assert(journal.at(0).timestampSeconds == 5);
        assert(journal.at(MemoryJournal::Capacity - 1).timestampSeconds
            == MemoryJournal::Capacity + 4);
    }
    {
        Genome curiousGenome = makeDefaultGenome();
        curiousGenome.personality.curiosity = 100;
        curiousGenome.personality.optimism = 100;
        Genome cautiousGenome = makeDefaultGenome();
        cautiousGenome.personality.curiosity = -100;
        cautiousGenome.personality.optimism = -100;
        CreatureSimulation curious(curiousGenome);
        CreatureSimulation cautious(cautiousGenome);
        const float curiousBondBefore = curious.affectionToPlayer();
        const float cautiousBondBefore = cautious.affectionToPlayer();
        curious.applyInteraction(Interaction::Novelty);
        cautious.applyInteraction(Interaction::Novelty);
        assert(curious.affectionToPlayer() - curiousBondBefore
            > cautious.affectionToPlayer() - cautiousBondBefore);
        assert(curious.emotions().value(Emotion::Curiosity)
            > cautious.emotions().value(Emotion::Curiosity));
        assert(curious.reaction().kind() == Reaction::Curious);
    }
    {
        CreatureSimulation creature;
        assert(creature.memories().size() == 1);
        assert(creature.memories().at(0).type == EventType::Birth);
        const float bondBefore = creature.affectionToPlayer();

        assert(creature.applyInteraction(Interaction::BroughtToForeground));
        assert(creature.isInForeground());
        const float bondAfterFirstVisit = creature.affectionToPlayer();
        assert(bondAfterFirstVisit >= bondBefore);

        // Pressing the Main Screen command again while already there does nothing.
        assert(!creature.applyInteraction(Interaction::BroughtToForeground));
        assert(creature.affectionToPlayer() == bondAfterFirstVisit);

        assert(creature.applyInteraction(Interaction::ReturnedToFarm));
        assert(!creature.isInForeground());
        assert(creature.applyInteraction(Interaction::BroughtToForeground));
        // A quick return-and-reopen does not grant another bond reward.
        assert(creature.affectionToPlayer() == bondAfterFirstVisit);
    }
    {
        CreatureSimulation creature;
        creature.applyInteraction(Interaction::Pet);
        creature.applyInteraction(Interaction::Feed);
        assert(creature.memories().size() == 1); // Routine care is not chronicle material.

        advance(creature, CreatureSimulation::SignificantEventCooldownSeconds);
        creature.applyInteraction(Interaction::Novelty);
        assert(creature.memories().size() == 2);

        creature.applyInteraction(Interaction::Novelty);
        creature.applyInteraction(Interaction::Pet);
        assert(creature.memories().size() == 2); // At most one significant event per 10 minutes.

        advance(creature, CreatureSimulation::SignificantEventCooldownSeconds);
        Genome sensitiveGenome = makeDefaultGenome();
        sensitiveGenome.personality.sensitivity = 100;
        CreatureSimulation sensitive(sensitiveGenome);
        advance(sensitive, CreatureSimulation::SignificantEventCooldownSeconds);
        sensitive.applyInteraction(Interaction::LoudSound);
        assert(sensitive.memories().size() == 2);
        assert(sensitive.memories().at(1).type == EventType::Startled);
    }
    {
        CreatureSimulation creature;
        creature.applyInteraction(Interaction::LoudSound);
        assert(creature.reaction().kind() == Reaction::Startled);
        advance(creature, 2.0F);
        assert(!creature.reaction().active());
    }

    std::cout << "All EvolvingGarden simulation tests passed.\n";
    return 0;
}
