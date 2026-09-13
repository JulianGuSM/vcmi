**VCMI Adventure Map Right-Click Previews**

English | [中文](README.zh-CN.md)

This version adds adventure map information previews to the VCMI engine. Hold the right mouse button over a visible location or neutral monster stack to view the information below without visiting it first.

| Object | Added functionality |
|---|---|
| Towns | Spell icons appear below the original town panel, grouped from the lowest spell level to the highest. Built guild levels use color icons; unbuilt or forbidden levels use dim grayscale icons. Library bonus spells also reflect the building's construction state. All icons appear together, wrapping and shrinking when needed, without scrolling, level labels or unbuilt-status text. |
| Shrines | A centered icon below the original text reveals the offered spell and whether the selected hero has learned it. |
| Witch huts | A centered icon below the original text reveals the offered secondary skill and whether the selected hero has learned it. |
| Scholars | Icons reveal the offered primary attribute, secondary skill or spell. Skills and spells show the selected hero's learned status. If the hero cannot learn the original offer, a separate row shows the substitute reward selected by the existing rules. |
| Creature banks | Crypts, Griffin Conservatories, Medusa Stores, Shipwrecks, Derelict Ships, Dragon Utopias and other creature banks display their actual defender groups. Each army slot has its own portrait and count, including repeated creature types. Reward icons and amounts appear below. Claimed rewards are no longer presented as available loot. |
| Pandora's Boxes | Unopened boxes reveal reward icons for resources, experience, creatures, artifacts, spells and other supported rewards. Guarded boxes show defender groups above the rewards; unguarded boxes show the rewards directly. |
| Black markets | Icons and names reveal the artifacts currently in stock, omitting empty slots left by purchases. An empty market displays a message. |
| Spell scrolls on the map | The popup reveals the scroll's spell name and icon, together with the selected hero's learned status. |
| Neutral monsters | The top of the popup shows predicted combat groups with exact counts. A line below shows the exact total and creature name, such as "112 Lizardmen". Creature type, level and threat details remain when UI enhancements are enabled, followed by the encounter disposition at the bottom. |

Monster disposition uses the selected hero and the engine's actual encounter rules to predict free joining, joining for a stated gold price, fleeing or fighting. Visions is not required. A paid offer shows the asking price; payment and army capacity are still handled during the actual encounter.

With no selected hero, spell and skill captions display "(No hero selected)". Monster totals remain exact, while formation details and disposition that require a hero prompt for one. Switching heroes and reopening a popup updates the relevant information. A unique upgraded creature is shown in its predicted group; modded creatures with multiple random upgrade options use a question mark for the unresolved creature type.

The interface changes also include:

- Centered, single-line learning-status captions, keeping the closing parenthesis of the Chinese no-hero message on the same line.
- A full square experience reward icon, fixing the excessive blank area inside its gold frame, with the amount centered below.
- A restored quantity-and-name line for neutral monsters, using the sum of the displayed groups instead of an approximate quantity range.
- English and Chinese translations for the new messages.

Previews read information already stored in the location. They do not trigger visits, grant rewards, change combat or diplomacy rules, or reveal fogged map areas. Town spell lists describe what the town offers; learning still depends on requirements such as a spellbook and Wisdom. Random rewards and reward choices are identified as such. Rewards generated dynamically by visit scripts are outside the static preview's scope.
