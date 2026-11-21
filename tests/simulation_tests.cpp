#include "creatures/genome.hpp"
#include "simulation/creature_simulation.hpp"
#include "simulation/memory.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
using namespace evolvinggarden;
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
    }
    {
        MemoryJournal journal;
        assert(!journal.add(MemoryEvent{0, EventType::Fed, 0, 10, 0}));
        for (std::uint32_t i = 0; i < MemoryJournal::Capacity + 5; ++i)
            journal.add(MemoryEvent{i, EventType::NoveltyDiscovered, 20, 50, 0});
        assert(journal.size() == MemoryJournal::Capacity);
        assert(journal.at(0).timestampSeconds == 5);
        assert(journal.at(MemoryJournal::Capacity - 1).timestampSeconds == MemoryJournal::Capacity + 4);
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
        curious.applyInteraction(Interaction::Novelty);
        cautious.applyInteraction(Interaction::Novelty);
        assert(curious.affectionToPlayer() > cautious.affectionToPlayer());
        assert(curious.emotions().value(Emotion::Curiosity) > cautious.emotions().value(Emotion::Curiosity));
    }
    {
        CreatureSimulation creature;
        assert(creature.memories().size() == 1);
        assert(creature.memories().at(0).type == EventType::Birth);
        creature.applyInteraction(Interaction::BroughtToForeground);
        assert(creature.memories().size() >= 2);
    }
    std::cout << "All EvolvingGarden simulation tests passed.\\n";
    return 0;
}
