/*
 * InfoWindows.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "InfoWindows.h"

#include "../CPlayerInterface.h"
#include "../PlayerLocalState.h"

#include "../adventureMap/AdventureMapInterface.h"
#include "../adventureMap/CMinimap.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/CursorHandler.h"
#include "../gui/Shortcut.h"
#include "../gui/WindowHandler.h"
#include "../widgets/Buttons.h"
#include "../widgets/CComponent.h"
#include "../widgets/GraphicalPrimitiveCanvas.h"
#include "../widgets/Images.h"
#include "../widgets/MiscWidgets.h"
#include "../widgets/TextControls.h"
#include "../widgets/Slider.h"
#include "CMessage.h"
#include "render/Canvas.h"
#include "render/CanvasImage.h"
#include "render/CAnimation.h"
#include "render/IImage.h"
#include "render/IRenderHandler.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/gameState/InfoAboutArmy.h"
#include "../../lib/mapObjects/CGCreature.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CGMarket.h"
#include "../../lib/mapObjects/CGTownInstance.h"
#include "../../lib/mapObjects/CRewardableObject.h"
#include "../../lib/mapObjects/Quest.h"
#include "../../lib/mapObjects/MiscObjects.h"
#include "../../lib/ConditionalWait.h"
#include "../../lib/entities/artifact/CArtifactInstance.h"

#include <boost/algorithm/string/trim.hpp>

CSelWindow::CSelWindow( const std::string & Text, PlayerColor player, int charperline, const std::vector<std::shared_ptr<CSelectableComponent>> & comps, const std::vector<std::pair<AnimationPath, CFunctionList<void()>>> & Buttons, QueryID askID)
{
	OBJECT_CONSTRUCTION;

	backgroundTexture = std::make_shared<CFilledTexture>(ImagePath::builtin("DiBoxBck"), pos);

	ID = askID;
	for(int i = 0; i < Buttons.size(); i++)
	{
		buttons.push_back(std::make_shared<CButton>(Point(0, 0), Buttons[i].first, CButton::tooltip(), Buttons[i].second));
		if(!i && askID.getNum() >= 0)
			buttons.back()->addCallback(std::bind(&CSelWindow::madeChoice, this));
		buttons[i]->addCallback(std::bind(&CInfoWindow::close, this)); //each button will close the window apart from call-defined actions
	}

	text = std::make_shared<CTextBox>(Text, Rect(0, 0, 250, 100), 0, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE);

	if(buttons.size() > 1 && askID.getNum() >= 0) //cancel button functionality
		buttons.back()->addCallback([askID](){GAME->interface()->cb->selectionMade(0, askID);});

	if(buttons.size() == 1)
		buttons.front()->assignedKey = EShortcut::GLOBAL_RETURN;

	if(buttons.size() == 2)
	{
		buttons.front()->assignedKey = EShortcut::GLOBAL_ACCEPT;
		buttons.back()->assignedKey = EShortcut::GLOBAL_CANCEL;
	}

	if(!comps.empty())
	{
		components = std::make_shared<CComponentBox>(comps, Rect(0,0,0,0));
		for (auto & comp : comps)
			comp->onChoose = [this](){ madeChoiceAndClose(); };
	}

	CMessage::drawIWindow(this, Text, player);
}

void CSelWindow::madeChoice()
{
	if(ID.getNum() < 0)
		return;
	int ret = -1;
	if(components)
		ret = components->selectedIndex();

	GAME->interface()->cb->selectionMade(ret + 1, ID);
}

void CSelWindow::madeChoiceAndClose()
{
	madeChoice();
	close();
}

CInfoWindow::CInfoWindow(const std::string & Text, PlayerColor player, const TCompsInfo & comps, const TButtonsInfo & Buttons)
{
	OBJECT_CONSTRUCTION;

	backgroundTexture = std::make_shared<CFilledTexture>(ImagePath::builtin("DiBoxBck"), pos);

	ID = QueryID(-1);
	for(const auto & Button : Buttons)
	{
		auto button = std::make_shared<CButton>(Point(0, 0), Button.first, CButton::tooltip(), std::bind(&CInfoWindow::close, this));
		button->setBorderColor(Colors::METALLIC_GOLD);
		button->addCallback(Button.second); //each button will close the window apart from call-defined actions
		buttons.push_back(button);
	}

	text = std::make_shared<CTextBox>(Text, Rect(0, 0, 250, 100), 0, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE);
	if(!text->slider)
	{
		int finalWidth = std::min(250, text->label->textSize.x + 32);
		int finalHeight = text->label->textSize.y;
		text->resize(Point(finalWidth, finalHeight));
	}

	if(buttons.size() == 1)
		buttons.front()->assignedKey = EShortcut::GLOBAL_RETURN;

	if(buttons.size() == 2)
	{
		buttons.front()->assignedKey = EShortcut::GLOBAL_ACCEPT;
		buttons.back()->assignedKey = EShortcut::GLOBAL_CANCEL;
	}

	if(!comps.empty())
		components = std::make_shared<CComponentBox>(comps, Rect(0,0,0,0));

	CMessage::drawIWindow(this, Text, player);
}

CInfoWindow::CInfoWindow()
{
	ID = QueryID(-1);
}

void CInfoWindow::close()
{
	WindowBase::close();

	if(GAME->interface())
		GAME->interface()->showingDialog->setFree();
}

void CInfoWindow::showAll(Canvas & to)
{
	CIntObject::showAll(to);
	CMessage::drawBorder(GAME->interface() ? GAME->interface()->playerID : PlayerColor(1), to, pos.w+28, pos.h+29, pos.x-14, pos.y-15);
}

CInfoWindow::~CInfoWindow() = default;

void CInfoWindow::showInfoDialog(const std::string & text, const TCompsInfo & components, PlayerColor player)
{
	ENGINE->windows().pushWindow(CInfoWindow::create(text, player, components));
}

void CInfoWindow::showYesNoDialog(const std::string & text, const TCompsInfo & components, const CFunctionList<void()> & onYes, const CFunctionList<void()> & onNo, PlayerColor player)
{
	assert(!GAME->interface() || GAME->interface()->showingDialog->isBusy());
	std::vector<std::pair<AnimationPath, CFunctionList<void()>>> pom;
	pom.emplace_back(AnimationPath::builtin("IOKAY.DEF"), nullptr);
	pom.emplace_back(AnimationPath::builtin("ICANCEL.DEF"), nullptr);
	auto temp = std::make_shared<CInfoWindow>(text, player, components, pom);

	temp->buttons[0]->addCallback(onYes);
	temp->buttons[1]->addCallback(onNo);

	ENGINE->windows().pushWindow(temp);
}

std::shared_ptr<CInfoWindow> CInfoWindow::create(const std::string & text, PlayerColor playerID, const TCompsInfo & components)
{
	std::vector<std::pair<AnimationPath, CFunctionList<void()>>> pom;
	pom.emplace_back(AnimationPath::builtin("IOKAY.DEF"), nullptr);
	return std::make_shared<CInfoWindow>(text, playerID, components, pom);
}

std::string CInfoWindow::genText(const std::string & title, const std::string & description)
{
	return std::string("{") + title + "}" + "\n\n" + description;
}

bool CRClickPopup::isPopupWindow() const
{
	return true;
}

void CRClickPopup::createAndPush(const std::string & txt, const CInfoWindow::TCompsInfo & comps)
{
	PlayerColor player = GAME->interface() ? GAME->interface()->playerID : PlayerColor(1); //if no player, then use blue
	if(settings["session"]["spectate"].Bool()) //TODO: there must be better way to implement this
		player = PlayerColor(1);

	auto temp = std::make_shared<CInfoWindow>(txt, player, comps);
	temp->center(ENGINE->getCursorPosition()); //center on mouse
#ifdef VCMI_MOBILE
	temp->moveBy({0, -temp->pos.h / 2});
#endif
	temp->fitToScreen(10);

	ENGINE->windows().createAndPushWindow<CRClickPopupInt>(temp);
}

void CRClickPopup::createAndPush(const std::string & txt, const std::shared_ptr<CComponent> & component)
{
	CInfoWindow::TCompsInfo intComps;
	intComps.push_back(component);

	createAndPush(txt, intComps);
}

void CRClickPopup::createAndPush(const CGObjectInstance * obj, const Point & p, ETextAlignment alignment)
{
	auto iWin = createCustomInfoWindow(p, obj); //try get custom infowindow for this obj
	if(iWin)
	{
		ENGINE->windows().pushWindow(iWin);
	}
	else
	{
		std::vector<Component> components;
		if(settings["general"]["enableUiEnhancements"].Bool())
		{
			if(GAME->interface()->localState->getCurrentHero())
				components = obj->getPopupComponents(GAME->interface()->localState->getCurrentHero());
			else
				components = obj->getPopupComponents(GAME->interface()->playerID);
		}

		std::vector<std::shared_ptr<CComponent>> guiComponents;
		for(auto & component : components)
			guiComponents.push_back(std::make_shared<CComponent>(component, CComponent::medium));

		if(GAME->interface()->localState->getCurrentHero())
			CRClickPopup::createAndPush(obj->getPopupText(GAME->interface()->localState->getCurrentHero()).toString(&GAME->translator()), guiComponents);
		else
			CRClickPopup::createAndPush(obj->getPopupText(GAME->interface()->playerID).toString(&GAME->translator()), guiComponents);
	}
}

CRClickPopupInt::CRClickPopupInt(const std::shared_ptr<CIntObject> & our) :
	dragDistance(Point(0, 0))
{
	addUsedEvents(DRAG_POPUP);

	ENGINE->cursor().hide();
	inner = our;
	addChild(our.get(), false);
}

CRClickPopupInt::~CRClickPopupInt()
{
	ENGINE->cursor().show();
}

void CRClickPopupInt::mouseDraggedPopup(const Point & cursorPosition, const Point & lastUpdateDistance)
{
	if(!settings["adventure"]["rightButtonDrag"].Bool())
		return;
	
	dragDistance += lastUpdateDistance;
	
	if(dragDistance.length() > 16)
		close();
}

template<typename... Args>
	requires (sizeof...(Args) != 1 || (!std::is_base_of_v<AdventureMapPopup, std::remove_cvref_t<Args>> && ...))
AdventureMapPopup::AdventureMapPopup(Args&&... args) :
	CWindowObject(std::forward<Args>(args)...), dragDistance(Point(0, 0))
{
	addUsedEvents(DRAG_POPUP);
}

void AdventureMapPopup::mouseDraggedPopup(const Point & cursorPosition, const Point & lastUpdateDistance)
{
	if(!settings["adventure"]["rightButtonDrag"].Bool())
		return;
	
	dragDistance += lastUpdateDistance;
	
	if(dragDistance.length() > 16)
		close();
}


namespace
{
std::string adventurePreviewText(const std::string & key)
{
	return MetaString::createFromTextID("vcmi.adventureMap.preview." + key).toString(&GAME->translator());
}
}

void CAdventureDetailsPopup::initPreview(int preferredWidth)
{
	OBJECT_CONSTRUCTION;
	pos.w = std::min(std::max(220, preferredWidth), ENGINE->screenDimensions().x - 60);
	filledBackground = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, 1));
}

void CAdventureDetailsPopup::addText(const std::string & text, bool title)
{
	if(text.empty())
		return;
	OBJECT_CONSTRUCTION;
	auto label = std::make_shared<CMultiLineLabel>(Rect(12, nextY, pos.w - 24, 1), title ? FONT_MEDIUM : FONT_SMALL,
		ETextAlignment::TOPCENTER, title ? Colors::METALLIC_GOLD : Colors::WHITE, text);
	label->pos.h = label->textSize.y;
	label->setVisibleSize(Rect(0, 0, label->pos.w, label->pos.h), false);
	nextY += label->pos.h + 12;
	widgets.push_back(label);
}

void CAdventureDetailsPopup::addComponents(const std::vector<Component> & components, bool largeIcons, const std::vector<std::string> & subtitles, bool wrapSubtitles)
{
	OBJECT_CONSTRUCTION;
	constexpr int gap = 8;
	std::vector<std::shared_ptr<CComponent>> row;
	int rowWidth = 0;
	int rowHeight = 0;
	auto finishRow = [&]()
	{
		if(row.empty())
			return;
		int x = (pos.w - rowWidth) / 2;
		for(const auto & component : row)
		{
			component->moveTo(pos.topLeft() + Point(x, nextY), true);
			auto border = std::make_shared<GraphicalPrimitiveCanvas>(component->image->pos - pos.topLeft());
			border->addRectangle(Point(0, 0), component->image->pos.dimensions(), Colors::METALLIC_GOLD);
			widgets.push_back(border);
			x += component->pos.w + gap;
		}
		nextY += rowHeight + 12;
		row.clear();
		rowWidth = 0;
		rowHeight = 0;
	};
	for(size_t index = 0; index < components.size(); ++index)
	{
		const auto & data = components[index];
		std::string subtitle;
		if(index < subtitles.size())
			subtitle = subtitles[index];
		else if(data.value && (data.type == ComponentType::CREATURE || data.type == ComponentType::RESOURCE
			|| data.type == ComponentType::EXPERIENCE || data.type == ComponentType::MANA || data.type == ComponentType::LEVEL))
			subtitle = std::to_string(*data.value);
		else if(data.type == ComponentType::SPELL_SCROLL)
		{
			auto description = MetaString::createFromTextID("vcmi.adventureMap.preview.scroll");
			description.appendName(data.subType.as<SpellID>());
			subtitle = description.toString(&GAME->translator());
		}
		// PSKIL32 leaves space below the experience emblem; PSKIL42 fills the framed reward icon.
		const bool useMediumIcon = largeIcons || data.type == ComponentType::EXPERIENCE || data.type == ComponentType::LEVEL;
		auto component = std::make_shared<CComponent>(data, useMediumIcon ? CComponent::medium : CComponent::small, FONT_SMALL, subtitle, wrapSubtitles);
		if(!row.empty() && rowWidth + gap + component->pos.w > pos.w - 24)
			finishRow();
		rowWidth += (row.empty() ? 0 : gap) + component->pos.w;
		rowHeight = std::max(rowHeight, component->pos.h);
		row.push_back(component);
		widgets.push_back(component);
	}
	finishRow();
}

void CAdventureDetailsPopup::addLearningComponents(const std::vector<Component> & components)
{
	const auto * hero = GAME->interface()->localState->getCurrentHero();
	std::vector<std::string> subtitles;
	for(const auto & component : components)
	{
		if(component.type != ComponentType::SPELL && component.type != ComponentType::SPELL_SCROLL && component.type != ComponentType::SEC_SKILL)
		{
			// Attribute rewards keep their normal amount/name rather than a learned status.
			subtitles.emplace_back();
			continue;
		}
		if(!hero)
		{
			subtitles.push_back(adventurePreviewText("noHero"));
			continue;
		}
		const bool known = component.type == ComponentType::SEC_SKILL
			? hero->getSecSkillLevel(component.subType.as<SecondarySkill>()) > 0
			: hero->spellbookContainsSpell(component.subType.as<SpellID>());
		subtitles.push_back(adventurePreviewText(known ? "learned" : "notLearned"));
	}
	addComponents(components, true, subtitles, false);
}

void CAdventureDetailsPopup::finishPreview(Point position)
{
	pos.h = nextY + 4;
	filledBackground->pos.h = pos.h;
	updateShadow();
	center(position);
	fitToScreen(26); // include the external dialog border in the screen margin
}

CAdventureDetailsPopup::CAdventureDetailsPopup(Point position, const CRewardableObject * object)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	const auto * hero = GAME->interface()->localState->getCurrentHero();
	if(object->hasLearningPreview())
	{
		initPreview(220);
		// Retain the normal name, description and visited state above the new icon.
		addText((hero ? object->getPopupText(hero) : object->getPopupText(GAME->interface()->playerID)).toString(&GAME->translator()));
		const auto contents = object->getLearningPreview();
		addLearningComponents(contents);
		if(contents.empty())
			addText(adventurePreviewText("none"));
		const auto fallback = object->getScholarFallbackPreview(hero);
		if(!fallback.empty())
		{
			addText(adventurePreviewText("scholarFallback"));
			addComponents(fallback, true);
		}
	}
	else
	{
		const auto preview = object->getBankPreview(hero);
		size_t largestRow = preview.guards.size();
		for(const auto & reward : preview.rewards)
			largestRow = std::max(largestRow, reward.size());
		initPreview(20 + 40 * static_cast<int>(std::min<size_t>(7, largestRow)));
		addText(object->getObjectName().toString(&GAME->translator()), true);
		addComponents(preview.guards);
		if(preview.cleared)
			addText(adventurePreviewText("cleared"));
		else if(preview.rewards.empty())
			addText(adventurePreviewText("noReward"));
		else
		{
			if(preview.rewards.size() > 1 && preview.selectMode != Rewardable::SELECT_ALL)
				addText(adventurePreviewText(preview.selectMode == Rewardable::SELECT_RANDOM ? "randomReward" : "chooseReward"));
			for(const auto & reward : preview.rewards)
			{
				if(reward.empty())
					addText(adventurePreviewText("none"));
				else
					addComponents(reward);
			}
		}
	}
	finishPreview(position);
}

CAdventureDetailsPopup::CAdventureDetailsPopup(Point position, const CGCreature * creature)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	const auto * hero = GAME->interface()->localState->getCurrentHero();
	const auto stacks = creature->getBattlePreview(hero);
	initPreview(20 + 40 * static_cast<int>(std::min<size_t>(7, stacks.size())));
	MetaString name;
	name.appendName(creature->getCreatureID(), 2);
	addText(name.toString(&GAME->translator()), true);
	std::vector<Component> components;
	std::vector<std::string> subtitles;
	bool randomUpgrade = false;
	TQuantity totalCount = 0;
	for(const auto & stack : stacks)
	{
		components.emplace_back(ComponentType::CREATURE, stack.type, stack.count);
		subtitles.push_back(std::to_string(stack.count) + (stack.randomUpgrade ? " ?" : ""));
		randomUpgrade = randomUpgrade || stack.randomUpgrade;
		totalCount += stack.count;
	}
	addComponents(components, false, subtitles);
	// Restore the quantity/name line with the exact total of the displayed stacks.
	if(totalCount > 0)
	{
		MetaString quantity;
		quantity.appendNumber(totalCount);
		quantity.appendRawString(" ");
		quantity.appendName(creature->getCreatureID(), totalCount);
		addText(quantity.toString(&GAME->translator()));
	}
	addText(boost::algorithm::trim_copy(creature->getPreviewDescription(hero).toString(&GAME->translator())));
	if(randomUpgrade)
		addText(adventurePreviewText("randomUpgrade"));
	if(!hero && creature->stacksCount <= 0 && stacks.size() == 1)
		addText(adventurePreviewText("selectHeroForStacks"));
	addText(creature->getEncounterPreviewText(hero).toString(&GAME->translator()));
	finishPreview(position);
}

CAdventureDetailsPopup::CAdventureDetailsPopup(Point position, const CGBlackMarket * market)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	std::vector<Component> artifacts;
	// Purchased items leave empty slots. Read the current stock without restocking it.
	for(const auto artifact : market->artifacts)
		if(artifact.hasValue())
			artifacts.emplace_back(ComponentType::ARTIFACT, artifact);
	initPreview(20 + 60 * static_cast<int>(std::min<size_t>(7, artifacts.size())));
	addText(market->getObjectName().toString(&GAME->translator()), true);
	if(artifacts.empty())
		addText(adventurePreviewText("marketEmpty"));
	else
		addComponents(artifacts);
	finishPreview(position);
}

CAdventureDetailsPopup::CAdventureDetailsPopup(Point position, const CGArtifact * scroll)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	initPreview(220);
	addText(scroll->getObjectName().toString(&GAME->translator()), true);
	const auto * artifact = scroll->getArtifactInstance();
	const SpellID spell = artifact ? artifact->getScrollSpellID() : SpellID();
	if(spell.hasValue())
	{
		MetaString name;
		name.appendName(spell);
		addText(name.toString(&GAME->translator()));
		addLearningComponents({Component(ComponentType::SPELL_SCROLL, spell)});
	}
	else
		addText(adventurePreviewText("none"));
	finishPreview(position);
}

CInfoBoxPopup::CInfoBoxPopup(Point position, const CGTownInstance * town)
	: AdventureMapPopup(RCLICK_POPUP | PLAYER_COLORED, ImagePath::builtin("TOWNQVBK"), position)
{
	InfoAboutTown iah;
	GAME->interface()->cb->getTownInfo(town, iah, GAME->interface()->localState->getCurrentArmy()); //todo: should this be nearest hero?

	OBJECT_CONSTRUCTION;
	if(settings["general"]["enableUiEnhancements"].Bool())
		background->setPlayerColor(town->getOwner());

	const auto levels = town->getSpellPreview();
	int largestLevel = 0;
	for(const auto & level : levels)
	{
		const int count = static_cast<int>(level.spells.size()) + (level.librarySpell ? 1 : 0);
		largestLevel = std::max(largestLevel, count);
	}
	if(largestLevel == 0)
	{
		tooltip = std::make_shared<CTownTooltip>(Point(9, 10), iah);
		fitToScreen(10);
		return;
	}

	// Keep the original town artwork and all CTownTooltip coordinates at native size.
	// Only the bottom frame is moved; its side edges continue around the spell panel.
	constexpr int sideBorder = 9;
	constexpr int bottomBorder = 14;
	constexpr int padding = 12;
	constexpr int gap = 3;
	constexpr int levelGap = 6;
	constexpr int sectionPadding = 8;
	constexpr int screenMargin = 10;
	const int townHeight = background->pos.h - bottomBorder;
	const int contentWidth = pos.w - padding * 2;
	const int maxContentHeight = std::max(1, ENGINE->screenDimensions().y - screenMargin * 2 - townHeight - sectionPadding * 2 - bottomBorder);
	const auto scrollAnimation = ENGINE->renderHandler().loadAnimation(AnimationPath::builtin("SPELLSCR"), EImageBlitMode::COLORKEY);
	const auto scrollImage = scrollAnimation->getImage(0, 0);
	const Point nativeIconSize = scrollImage->dimensions();

	// Prefer one row per ordinary guild level (including a Library's sixth slot).
	// Large pools, such as Aurora Borealis, wrap and shrink until all rows fit.
	const int preferredColumns = std::clamp(largestLevel, 5, 6);
	int iconWidth = std::max(1, std::min(nativeIconSize.x, (contentWidth - gap * (preferredColumns - 1)) / preferredColumns));
	Point iconSize;
	int columns = 0;
	int contentHeight = 0;
	for(;;)
	{
		iconSize = Point(iconWidth, std::max(1, static_cast<int>(std::lround(static_cast<double>(nativeIconSize.y) * iconWidth / nativeIconSize.x))));
		columns = std::max(1, (contentWidth + gap) / (iconWidth + gap));
		contentHeight = 0;
		for(const auto & level : levels)
		{
			const int count = static_cast<int>(level.spells.size()) + (level.librarySpell ? 1 : 0);
			if(count == 0)
				continue;
			const int rows = (count + columns - 1) / columns;
			if(contentHeight > 0)
				contentHeight += levelGap;
			contentHeight += rows * iconSize.y + (rows - 1) * gap;
		}
		if(contentHeight <= maxContentHeight || iconWidth == 1)
			break;
		--iconWidth;
	}
	pos.h = townHeight + sectionPadding * 2 + contentHeight + bottomBorder;

	const auto original = background->getSurface();
	auto extended = ENGINE->renderHandler().createImage(Point(pos.w, pos.h), CanvasScalingPolicy::AUTO);
	auto canvas = extended->getCanvas();
	canvas.fillTexture(ENGINE->renderHandler().loadImage(ImagePath::builtin("DiBoxBck"), EImageBlitMode::OPAQUE));
	canvas.draw(original, Point(0, 0), Rect(0, 0, pos.w, townHeight));
	for(int y = townHeight; y < pos.h - bottomBorder; ++y)
	{
		canvas.draw(original, Point(0, y), Rect(0, 40, sideBorder, 1));
		canvas.draw(original, Point(pos.w - sideBorder, y), Rect(pos.w - sideBorder, 40, sideBorder, 1));
	}
	canvas.draw(original, Point(0, pos.h - bottomBorder), Rect(0, townHeight, pos.w, bottomBorder));
	background = std::make_shared<CPicture>(extended, Point(0, 0));
	tooltip = std::make_shared<CTownTooltip>(Point(9, 10), iah);

	// Each popup owns scaled normal/grayscale copies. Never recolor the shared
	// SPELLSCR frames used by the mage guild and other windows.
	std::map<SpellID, std::array<std::shared_ptr<CanvasImage>, 2>> spellImages;
	auto imageForSpell = [&](SpellID spell, bool available) -> std::shared_ptr<CanvasImage>
	{
		auto & variants = spellImages[spell];
		if(!variants[0])
		{
			variants[0] = ENGINE->renderHandler().createImage(iconSize, CanvasScalingPolicy::AUTO);
			auto normalCanvas = variants[0]->getCanvas();
			const Rect iconRect(Point(0, 0), iconSize);
			normalCanvas.drawColor(iconRect, Colors::TRANSPARENCY);
			if(const auto source = scrollAnimation->getImage(spell.getNum(), 0))
			{
				Canvas sourceCanvas(source->dimensions(), CanvasScalingPolicy::AUTO);
				sourceCanvas.drawColor(Rect(Point(0, 0), source->dimensions()), Colors::TRANSPARENCY);
				sourceCanvas.draw(source, Point(0, 0));
				normalCanvas.drawScaled(sourceCanvas, Point(0, 0), iconSize);
			}
			normalCanvas.drawBorder(iconRect, Colors::METALLIC_GOLD);
			normalCanvas.applyTransparency(true);

			variants[1] = ENGINE->renderHandler().createImage(iconSize, CanvasScalingPolicy::AUTO);
			auto grayCanvas = variants[1]->getCanvas();
			grayCanvas.drawColor(iconRect, Colors::TRANSPARENCY);
			grayCanvas.draw(variants[0], Point(0, 0));
			grayCanvas.applyGrayscale();
			// SPELLSCR already has mostly gray artwork: dim the unavailable copy
			// as well, so construction state remains visible without any labels.
			grayCanvas.drawColorBlended(iconRect, ColorRGBA(0, 0, 0, 64));
			grayCanvas.applyTransparency(true);
		}
		return variants[available ? 0 : 1];
	};

	spellContainer = std::make_shared<CIntObject>(0, Point(padding, townHeight + sectionPadding));
	spellContainer->pos.w = contentWidth;
	spellContainer->pos.h = contentHeight;
	{
		OBJECT_CONSTRUCTION_TARGETED(spellContainer.get());
		int y = 0;
		for(const auto & level : levels)
		{
			auto spells = level.spells;
			if(level.librarySpell)
				spells.push_back(*level.librarySpell);
			if(spells.empty())
				continue;
			if(y > 0)
				y += levelGap;
			for(size_t begin = 0; begin < spells.size(); begin += columns)
			{
				if(begin > 0)
					y += gap;
				const int count = static_cast<int>(std::min<size_t>(columns, spells.size() - begin));
				const int rowWidth = count * iconSize.x + (count - 1) * gap;
				for(int column = 0; column < count; ++column)
				{
					const size_t index = begin + column;
					const int x = (contentWidth - rowWidth) / 2 + column * (iconSize.x + gap);
					const bool library = level.librarySpell && index == level.spells.size();
					const bool available = level.built && !level.forbidden && !library;
					spellWidgets.push_back(std::make_shared<CPicture>(imageForSpell(spells[index], available), Point(x, y)));
				}
				y += iconSize.y;
			}
		}
	}

	updateShadow();
	center(position);
	fitToScreen(10);
}

CInfoBoxPopup::CInfoBoxPopup(Point position, const CGHeroInstance * hero)
	: AdventureMapPopup(RCLICK_POPUP | PLAYER_COLORED, ImagePath::builtin("HEROQVBK"), position)
{
	InfoAboutHero iah;
	GAME->interface()->cb->getHeroInfo(hero, iah, GAME->interface()->localState->getCurrentArmy()); //todo: should this be nearest hero?

	OBJECT_CONSTRUCTION;
	tooltip = std::make_shared<CHeroTooltip>(Point(9, 10), iah);

	if(settings["general"]["enableUiEnhancements"].Bool())
		background->setPlayerColor(hero->getOwner());
	
	addUsedEvents(DRAG_POPUP);

	fitToScreen(10);
}

CInfoBoxPopup::CInfoBoxPopup(Point position, const CGGarrison * garr)
	: AdventureMapPopup(RCLICK_POPUP | PLAYER_COLORED, ImagePath::builtin(settings["general"]["enableUiEnhancements"].Bool() ? "GARRIPOP" : "TOWNQVBK"), position)
{
	InfoAboutTown iah;
	GAME->interface()->cb->getTownInfo(garr, iah);

	OBJECT_CONSTRUCTION;

	if(settings["general"]["enableUiEnhancements"].Bool())
	{
        tooltip = std::make_shared<CGarrisonTooltip>(Point(9, 10), iah);
	}
	else
	{
        tooltip = std::make_shared<CArmyTooltip>(Point(9, 10), iah);
	}

	if(settings["general"]["enableUiEnhancements"].Bool())
		background->setPlayerColor(garr->getOwner());

	addUsedEvents(DRAG_POPUP);

	fitToScreen(10);
}

MinimapWithIcons::MinimapWithIcons(const Point & position)
{
	OBJECT_CONSTRUCTION;
	pos += position;

	recreate();
}

void MinimapWithIcons::recreate()
{
	OBJECT_CONSTRUCTION;

	Rect area1(11, 41, 144, 144);
	Rect area2(167, 41, 144, 144);

	Rect border1(10, 40, 147, 147);
	Rect border2(166, 40, 147, 147);

	int levels = GAME->interface()->cb->getMapSize().z;
	int currentLevel = slider ? slider->getValue() : 0;
	bool singleLevelMap = levels == 1;

	if(levels > 2)
	{
		slider = std::make_shared<CSlider>(Point(10, 192), 303, [this](int value){ recreate(); setRedrawParent(true); redraw(); }, 2, levels, currentLevel, Orientation::HORIZONTAL);
		slider->setPanningStep(147);
	}

	if (singleLevelMap)
	{
		area1.x += 78;
		border1.x += 78;
	}

	background1 = std::make_shared<TransparentFilledRectangle>(border1, Colors::TRANSPARENCY, Colors::YELLOW);
	map1 = std::make_shared<CMinimapInstance>(area1.topLeft(), area1.dimensions(), currentLevel);

	if (!singleLevelMap)
	{
		background2 = std::make_shared<TransparentFilledRectangle>(border2, Colors::TRANSPARENCY, Colors::YELLOW);
		map2 = std::make_shared<CMinimapInstance>(area2.topLeft(), area2.dimensions(), currentLevel + 1);
	}

	iconsOverlay.clear();
	for(const auto & icon : icons)
	{
		int positionX = 144 * icon.first.x / GAME->interface()->cb->getMapSize().x;
		int positionY = 144 * icon.first.y / GAME->interface()->cb->getMapSize().y;

		Point iconPosition(positionX, positionY);

		iconPosition -= Point(8,8); // compensate for 16x16 icon half-size

		if (icon.first.z == currentLevel)
			iconPosition += area1.topLeft();
		else if (icon.first.z == currentLevel + 1)
			iconPosition += area2.topLeft();
		else
			continue;

		iconsOverlay.push_back(std::make_shared<CPicture>(icon.second, iconPosition));
	}
}

void MinimapWithIcons::addIcon(const int3 & coordinates, const ImagePath & image )
{
	icons.push_back({coordinates, image});
}

TeleporterPopup::TeleporterPopup(const Point & position, const CGTeleport * teleporter)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	OBJECT_CONSTRUCTION;
	pos.w = 322;
	pos.h = 200 + (GAME->interface()->cb->getMapSize().z > 2 ? 21 : 0);

	filledBackground = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, pos.h));
	labelTitle = std::make_shared<CLabel>(pos.w / 2, 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE, teleporter->getPopupText(GAME->interface()->playerID).toString(&GAME->translator()));
	minimap = std::make_shared<MinimapWithIcons>(Point(0,0));

	const auto & entrances = teleporter->getAllEntrances();
	const auto & exits = teleporter->getAllExits();

	std::set<ObjectInstanceID> allTeleporters;
	allTeleporters.insert(entrances.begin(), entrances.end());
	allTeleporters.insert(exits.begin(), exits.end());

	for (const auto exit : allTeleporters)
	{
		const auto * exitObject = GAME->interface()->cb->getObj(exit, false);

		if (!exitObject)
			continue;

		int3 position = exitObject->visitablePos();
		ImagePath image;

		if (!vstd::contains(entrances, exit))
			image = ImagePath::builtin("minimapIcons/portalExit");
		else if (!vstd::contains(exits, exit))
			image = ImagePath::builtin("minimapIcons/portalEntrance");
		else
			image = ImagePath::builtin("minimapIcons/portalBidirectional");

		minimap->addIcon(position, image);
	}
	minimap->recreate();
	center(position);
	fitToScreen(10);
}

KeymasterPopup::KeymasterPopup(const Point & position, const CGObjectInstance * keyObject)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	OBJECT_CONSTRUCTION;
	pos.w = 322;
	pos.h = 220 + (GAME->interface()->cb->getMapSize().z > 2 ? 21 : 0);

	filledBackground = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, pos.h));
	labelTitle = std::make_shared<CLabel>(pos.w / 2, 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE, keyObject->getObjectName().toString(&GAME->translator()));
	labelDescription = std::make_shared<CLabel>(pos.w / 2, 40, FONT_SMALL, ETextAlignment::CENTER, Colors::WHITE, QuestSource::keymasterVisitedText(keyObject, GAME->interface()->playerID).toString(&GAME->translator()));
	minimap = std::make_shared<MinimapWithIcons>(Point(0,20));

	const auto allObjects = GAME->interface()->cb->getAllVisitableObjs();

	for (const auto mapObject : allObjects)
	{
		if (!mapObject)
			continue;

		switch (mapObject->ID)
		{
			case Obj::KEYMASTER:
				if (mapObject->subID == keyObject->subID)
					minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/keymaster"));
				break;
			case Obj::BORDERGUARD:
				if (mapObject->subID == keyObject->subID)
					minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/borderguard"));
				break;
			case Obj::BORDER_GATE:
				if (mapObject->subID == keyObject->subID)
					minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/bordergate"));
				break;
		}
	}
	minimap->recreate();
	center(position);
	fitToScreen(10);
}

ObeliskPopup::ObeliskPopup(const Point & position, const CGObelisk * obelisk)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	OBJECT_CONSTRUCTION;
	pos.w = 322;
	pos.h = 220 + (GAME->interface()->cb->getMapSize().z > 2 ? 21 : 0);

	filledBackground = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, pos.h));
	labelTitle = std::make_shared<CLabel>(pos.w / 2, 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE, obelisk->getObjectName().toString(&GAME->translator()));
	labelDescription = std::make_shared<CLabel>(pos.w / 2, 40, FONT_SMALL, ETextAlignment::CENTER, Colors::WHITE, obelisk->getObjectDescription(GAME->interface()->playerID).toString(&GAME->translator()));
	minimap = std::make_shared<MinimapWithIcons>(Point(0,20));

	const auto allObjects = GAME->interface()->cb->getAllVisitableObjs();

	for (const auto mapObject : allObjects)
	{
		if (!mapObject)
			continue;

		if (mapObject->ID != Obj::OBELISK)
			continue;

		if (mapObject->wasVisited(GAME->interface()->playerID))
			minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/obeliskVisited"));
		else
			minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/obelisk"));
	}
	minimap->recreate();
	center(position);
	fitToScreen(10);
}

SearchPopup::SearchPopup(std::vector<const CGObjectInstance *> objs)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	OBJECT_CONSTRUCTION;
	pos.w = 322;
	pos.h = 220 + (GAME->interface()->cb->getMapSize().z > 2 ? 21 : 0);

	if(!objs.size())
		return;

	auto name = GAME->interface()->cb->getObjInstance(objs.at(0)->id)->getObjectName().toString(&GAME->translator());

	filledBackground = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, pos.h));
	labelTitle = std::make_shared<CLabel>(pos.w / 2, 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE, name);
	minimap = std::make_shared<MinimapWithIcons>(Point(0,20));

	for (const auto obj : objs)
		minimap->addIcon(obj->visitablePos(), ImagePath::builtin("minimapIcons/generic"));
	
	minimap->recreate();
	center();
	fitToScreen(10);
}

std::shared_ptr<WindowBase>
CRClickPopup::createCustomInfoWindow(Point position, const CGObjectInstance * specific) //specific=0 => draws info about selected town/hero
{
	if(nullptr == specific)
		specific = GAME->interface()->localState->getCurrentArmy();

	if(nullptr == specific)
	{
		logGlobal->error("createCustomInfoWindow: no object to describe");
		return nullptr;
	}

	// Keep fog-of-war checks even though these previews bypass scouting/ownership.
	if (!GAME->interface()->cb->getObj(specific->id, false))
		return nullptr;

	if(const auto * rewardable = dynamic_cast<const CRewardableObject *>(specific);
		rewardable && (rewardable->hasBankPreview() || rewardable->hasLearningPreview()))
		return std::make_shared<CAdventureDetailsPopup>(position, rewardable);

	switch(specific->ID)
	{
		case Obj::HERO:
			return std::make_shared<CInfoBoxPopup>(position, dynamic_cast<const CGHeroInstance *>(specific));
		case Obj::TOWN:
			return std::make_shared<CInfoBoxPopup>(position, dynamic_cast<const CGTownInstance *>(specific));
		case Obj::MONSTER:
			return std::make_shared<CAdventureDetailsPopup>(position, dynamic_cast<const CGCreature *>(specific));
		case Obj::BLACK_MARKET:
			if(const auto * market = dynamic_cast<const CGBlackMarket *>(specific))
				return std::make_shared<CAdventureDetailsPopup>(position, market);
			return nullptr;
		case Obj::SPELL_SCROLL:
			if(const auto * scroll = dynamic_cast<const CGArtifact *>(specific))
				return std::make_shared<CAdventureDetailsPopup>(position, scroll);
			return nullptr;
		case Obj::GARRISON:
		case Obj::GARRISON2:
			return std::make_shared<CInfoBoxPopup>(position, dynamic_cast<const CGGarrison *>(specific));
		case Obj::MONOLITH_ONE_WAY_ENTRANCE:
		case Obj::MONOLITH_ONE_WAY_EXIT:
		case Obj::MONOLITH_TWO_WAY:
		case Obj::SUBTERRANEAN_GATE:
		case Obj::WHIRLPOOL:
			return std::make_shared<TeleporterPopup>(position, dynamic_cast<const CGTeleport *>(specific));
		case Obj::KEYMASTER:
		case Obj::BORDERGUARD:
		case Obj::BORDER_GATE:
			return std::make_shared<KeymasterPopup>(position, specific);
		case Obj::OBELISK:
			return std::make_shared<ObeliskPopup>(position, dynamic_cast<const CGObelisk *>(specific));
		default:
			return std::shared_ptr<WindowBase>();
	}
}
