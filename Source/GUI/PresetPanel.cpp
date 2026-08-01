// ==========================================
// File: PresetPanel.cpp
// SPECTRA8 - プリセットブラウザパネル (PicoSampler 準拠)
// ==========================================
#include "PresetPanel.h"

PresetPanel::PresetPanel(juce::AudioProcessorValueTreeState& apvts)
    : manager(apvts)
{
    txtSearch.setTextToShowWhenEmpty("Search presets...", juce::Colours::grey);
    txtSearch.onTextChange = [this] { refreshPresetList(); };
    addAndMakeVisible(txtSearch);

    btnSave.setColour(juce::TextButton::buttonColourId, SpectraColors::knobTrack);
    btnSave.setColour(juce::TextButton::textColourOffId, SpectraColors::text);
    btnInit.setColour(juce::TextButton::buttonColourId, SpectraColors::knobTrack);
    btnInit.setColour(juce::TextButton::textColourOffId, SpectraColors::textDim);

    addAndMakeVisible(btnSave);
    addAndMakeVisible(btnInit);

    categoryListModel.owner = this;
    presetListModel.owner   = this;
    categoryListModel.isCategoryList = true;

    lstCategories.setModel(&categoryListModel);
    lstPresets.setModel(&presetListModel);
    lstCategories.setColour(juce::ListBox::backgroundColourId, SpectraColors::panel);
    lstPresets.setColour(juce::ListBox::backgroundColourId, SpectraColors::panel);

    addAndMakeVisible(lstCategories);
    addAndMakeVisible(lstPresets);

    btnSave.onClick = [this] { showSaveDialog(); };
    btnInit.onClick = [this] { showInitDialog(); };

    refreshAll();
}

void PresetPanel::refreshAll()
{
    allPresets = manager.getAllPresets();

    categories.clear();
    categories.add("All");
    categories.add(juce::String::fromUTF8(u8"Favorites ★"));
    categories.addArray(manager.getCategories());

    categoryListModel.owner = this;
    lstCategories.updateContent();

    if (selectedCategoryRow >= categories.size()) selectedCategoryRow = 0;
    lstCategories.selectRow(selectedCategoryRow);

    refreshPresetList();
}

void PresetPanel::refreshPresetList()
{
    currentPresets.clear();

    const juce::String cat = (selectedCategoryRow >= 0 && selectedCategoryRow < categories.size())
                               ? categories[selectedCategoryRow]
                               : "All";

    const juce::String filter = txtSearch.getText().trim();
    const bool isFavCat = (cat == juce::String::fromUTF8(u8"Favorites ★"));

    for (const auto& item : allPresets)
    {
        bool matchesCategory = false;
        if (cat == "All")
            matchesCategory = true;
        else if (isFavCat)
            matchesCategory = item.isFavorite;
        else
            matchesCategory = (item.category == cat);

        if (matchesCategory)
        {
            if (filter.isEmpty() || item.name.containsIgnoreCase(filter))
            {
                currentPresets.add(item);
            }
        }
    }

    presetListModel.owner = this;
    lstPresets.updateContent();
    repaint();
}

void PresetPanel::categoryRowClicked(int row)
{
    if (row < 0 || row >= categories.size()) return;
    selectedCategoryRow = row;
    refreshPresetList();
}

void PresetPanel::presetRowClicked(int row)
{
    if (row >= 0 && row < currentPresets.size())
    {
        manager.loadPreset(currentPresets[row]);
    }
}

void PresetPanel::toggleFavoriteClicked(int row)
{
    if (row >= 0 && row < currentPresets.size())
    {
        manager.toggleFavorite(currentPresets[row]);
        refreshAll();
    }
}

void PresetPanel::presetRowRightClicked(int row)
{
    if (row < 0 || row >= currentPresets.size()) return;

    const auto item = currentPresets[row];
    lstPresets.selectRow(row);

    juce::PopupMenu menu;
    menu.addSectionHeader(item.name);
    menu.addItem(1, "Load");
    menu.addItem(2, item.isFavorite ? "Unmark Favorite" : juce::String::fromUTF8(u8"Mark Favorite ★"));

    if (!item.isFactory)
    {
        menu.addSeparator();
        menu.addItem(3, "Delete...");
    }

    juce::Component::SafePointer<PresetPanel> safeThis(this);

    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetComponent(&lstPresets)
                           .withMousePosition(),
        [safeThis, item](int result)
        {
            if (safeThis == nullptr) return;

            if (result == 1)
            {
                safeThis->manager.loadPreset(item);
            }
            else if (result == 2)
            {
                safeThis->manager.toggleFavorite(item);
                safeThis->refreshAll();
            }
            else if (result == 3)
            {
                safeThis->confirmDelete(item);
            }
        });
}

void PresetPanel::confirmDelete(const PresetItem& item)
{
    juce::Component::SafePointer<PresetPanel> safeThis(this);

    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Delete Preset",
        "Delete the preset \"" + item.name + "\"?\n\nThis cannot be undone.",
        "Yes",
        "No",
        this,
        juce::ModalCallbackFunction::create([safeThis, item](int result)
        {
            if (result != 1 || safeThis == nullptr) return;
            safeThis->manager.deletePreset(item);
            safeThis->refreshAll();
        }));
}

void PresetPanel::showSaveDialog()
{
    auto* win = new juce::AlertWindow("Save User Preset",
                                      "Choose a category and enter a preset name.",
                                      juce::MessageBoxIconType::NoIcon,
                                      this);

    juce::StringArray cats = manager.getCategories();
    win->addComboBox("category", cats, "Category");
    win->addTextEditor("name", lastSavedName, "Preset Name");
    win->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    win->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetPanel> safeThis(this);

    win->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, win](int result)
        {
            std::unique_ptr<juce::AlertWindow> owned(win);
            if (result != 1 || safeThis == nullptr) return;

            juce::String cat = "User";
            if (auto* cb = owned->getComboBoxComponent("category"))
            {
                const auto t = cb->getText().trim();
                if (t.isNotEmpty()) cat = t;
            }

            const auto name = owned->getTextEditorContents("name").trim();
            if (name.isNotEmpty())
            {
                safeThis->lastSavedName = name;
                safeThis->manager.saveUserPreset(cat, name);
                safeThis->refreshAll();
            }
        }), false);
}

void PresetPanel::showInitDialog()
{
    juce::Component::SafePointer<PresetPanel> safeThis(this);

    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Reset to Initial State",
        "This will reset every parameter to its default value.\nDo you want to continue?",
        "Yes",
        "No",
        this,
        juce::ModalCallbackFunction::create([safeThis](int result)
        {
            if (result == 1 && safeThis != nullptr)
            {
                safeThis->manager.initPreset();
            }
        }));
}

void PresetPanel::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);

    auto r = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(SpectraColors::panel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(SpectraColors::panelLine);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    // ListBoxの真上に見出しテキストを描画
    const int headerY = lstCategories.getY() - 16;
    g.setColour(SpectraColors::textDim);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("CATEGORY", lstCategories.getX(), headerY, lstCategories.getWidth(), 14, juce::Justification::left);
    g.drawText("PRESETS", lstPresets.getX(), headerY, lstPresets.getWidth(), 14, juce::Justification::left);
}

void PresetPanel::resized()
{
    auto area = getLocalBounds().reduced(24);

    // 上段: 検索窓・ボタン行 (28px)
    auto topRow = area.removeFromTop(28);

    txtSearch.setBounds(topRow.removeFromLeft(200));
    btnInit.setBounds(topRow.removeFromRight(64));
    topRow.removeFromRight(8);
    btnSave.setBounds(topRow.removeFromRight(64));

    // 見出しテキスト用マージン (24px)
    area.removeFromTop(24);

    // 下段: カテゴリリストとプリセットリスト
    auto catArea = area.removeFromLeft(180);
    area.removeFromLeft(16);

    lstCategories.setBounds(catArea);
    lstPresets.setBounds(area);
}

// ----------------------------------------------------------------------
// ListBoxModel
// ----------------------------------------------------------------------
int PresetPanel::SimpleListModel::getNumRows()
{
    if (owner == nullptr) return 0;
    return isCategoryList ? owner->categories.size() : owner->currentPresets.size();
}

void PresetPanel::SimpleListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (owner == nullptr) return;

    if (isCategoryList)
    {
        if (row < 0 || row >= owner->categories.size()) return;
        if (selected)
        {
            g.setColour(SpectraColors::accentVocoder.withAlpha(0.2f));
            g.fillRect(0, 0, w, h);
        }
        g.setColour(selected ? SpectraColors::accentVocoder : SpectraColors::text);
        g.setFont(juce::FontOptions(12.0f, selected ? juce::Font::bold : juce::Font::plain));
        g.drawText(owner->categories[row], 10, 0, w - 20, h, juce::Justification::centredLeft, true);
    }
    else
    {
        if (row < 0 || row >= owner->currentPresets.size()) return;
        const auto& item = owner->currentPresets[row];

        if (selected)
        {
            g.setColour(SpectraColors::accentVocoder.withAlpha(0.25f));
            g.fillRect(0, 0, w, h);
        }

        // お気に入り ★ / ☆ アイコン (UTF-8)
        g.setColour(item.isFavorite ? juce::Colour(0xFFFFD700) : SpectraColors::textDim.withAlpha(0.4f));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(item.isFavorite ? juce::String::fromUTF8(u8"★") : juce::String::fromUTF8(u8"☆"), 8, 0, 16, h, juce::Justification::centred);

        // プリセット名
        g.setColour(selected ? SpectraColors::text : SpectraColors::textDim);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(item.name, 30, 0, w - 110, h, juce::Justification::centredLeft, true);

        // 種別ラベル (FACTORY / USER)
        g.setColour(item.isFactory ? SpectraColors::accentVocoder.withAlpha(0.7f) : SpectraColors::mint.withAlpha(0.7f));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(item.isFactory ? "FACTORY" : "USER", w - 75, 0, 65, h, juce::Justification::centredRight, true);
    }
}

void PresetPanel::SimpleListModel::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (owner == nullptr) return;

    if (isCategoryList)
    {
        owner->categoryRowClicked(row);
        return;
    }

    if (e.x <= 26) // ★ アイコンクリック
    {
        owner->toggleFavoriteClicked(row);
        return;
    }

    if (e.mods.isPopupMenu())
        owner->presetRowRightClicked(row);
}

void PresetPanel::SimpleListModel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (owner == nullptr || isCategoryList) return;
    owner->presetRowClicked(row);
}

void PresetPanel::SimpleListModel::returnKeyPressed(int row)
{
    if (owner == nullptr || isCategoryList) return;
    owner->presetRowClicked(row);
}
