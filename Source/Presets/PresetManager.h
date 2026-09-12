// ==========================================
// File: PresetManager.h
// SPECTRA8 - プリセット管理・ファイル I/O・お気に入り管理
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <set>
#include "FactoryPresets.h"

struct PresetItem
{
    juce::String name;
    juce::String category;
    juce::File file;          // ユーザープリセット用
    bool isFactory = false;
    bool isFavorite = false;
};

class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& vts)
        : apvts(vts)
    {
        userPresetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("OTODESK/SPECTRA8/Presets");
        if (!userPresetDir.exists())
            userPresetDir.createDirectory();

        favoritesFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("OTODESK/SPECTRA8/favorites.txt");

        loadFavorites();
    }

    // ディレクトリとお気に入りから最新のプリセット一覧を生成
    juce::Array<PresetItem> getAllPresets()
    {
        juce::Array<PresetItem> list;

        // 1. ファクトリープリセット
        for (const auto& fp : FactoryPresets::getPresets())
        {
            const std::string key = ("Factory:" + fp.category + ":" + fp.name).toStdString();
            PresetItem item;
            item.name = fp.name;
            item.category = fp.category;
            item.isFactory = true;
            item.isFavorite = (favorites.find(key) != favorites.end());
            list.add(item);
        }

        // 2. ユーザープリセット (サブフォルダごと)
        if (userPresetDir.exists())
        {
            juce::Array<juce::File> presetFiles;
            userPresetDir.findChildFiles(presetFiles, juce::File::findFiles, true, "*.spectra8preset");

            for (const auto& file : presetFiles)
            {
                PresetItem item;
                item.name = file.getFileNameWithoutExtension();
                item.category = file.getParentDirectory().getFileName();
                if (item.category == "Presets") item.category = "User";
                item.file = file;
                item.isFactory = false;

                const std::string key = ("User:" + item.category + ":" + item.name).toStdString();
                item.isFavorite = (favorites.find(key) != favorites.end());
                list.add(item);
            }
        }

        return list;
    }

    juce::StringArray getCategories()
    {
        juce::StringArray cats;
        cats.add("FilterBank (Auto)");
        cats.add("LPC (Auto)");
        cats.add("FilterBank (MIDI)");
        cats.add("LPC (MIDI)");
        cats.add("M.Pitch Modulations");
        cats.add("Rhythmic Formant Gate");
        cats.add("Spectral Resonator Lab");
        cats.add("SpecialFX");
        cats.add("User");
        return cats;
    }

    void toggleFavorite(const PresetItem& item)
    {
        const juce::String prefix = item.isFactory ? "Factory:" : "User:";
        const std::string key = (prefix + item.category + ":" + item.name).toStdString();

        if (favorites.find(key) != favorites.end())
            favorites.erase(key);
        else
            favorites.insert(key);

        saveFavorites();
    }

    void loadPreset(const PresetItem& item)
    {
        if (item.isFactory)
        {
            // ファクトリープリセットのパラメータ適用
            for (const auto& fp : FactoryPresets::getPresets())
            {
                if (fp.category == item.category && fp.name == item.name)
                {
                    // まず安全のためデフォルト値へリセット
                    initPreset();

                    // 各パラメータをロード
                    for (const auto& kv : fp.params)
                    {
                        if (auto* param = apvts.getParameter(kv.first))
                        {
                            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(param))
                            {
                                const float norm = rp->convertTo0to1(kv.second);
                                param->beginChangeGesture();
                                param->setValueNotifyingHost(norm);
                                param->endChangeGesture();
                            }
                        }
                    }
                    break;
                }
            }
        }
        else if (item.file.existsAsFile())
        {
            // ユーザープリセット (XML)
            std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(item.file));
            if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
            {
                apvts.replaceState(juce::ValueTree::fromXml(*xml));
            }
        }
    }

    void saveUserPreset(const juce::String& category, const juce::String& name)
    {
        juce::File targetDir = userPresetDir.getChildFile(category.trim().isEmpty() ? "User" : category.trim());
        if (!targetDir.exists())
            targetDir.createDirectory();

        juce::File file = targetDir.getChildFile(juce::File::createLegalFileName(name) + ".spectra8preset");

        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        if (xml != nullptr)
        {
            xml->writeTo(file);
        }
    }

    bool deletePreset(const PresetItem& item)
    {
        if (item.isFactory || !item.file.existsAsFile())
            return false;

        return item.file.deleteFile();
    }

    void initPreset()
    {
        auto state = apvts.copyState();
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild(i);
            if (auto* param = apvts.getParameter(child.getProperty("id").toString()))
            {
                const float defVal = param->getDefaultValue();
                param->beginChangeGesture();
                param->setValueNotifyingHost(defVal);
                param->endChangeGesture();
            }
        }
    }

private:
    void loadFavorites()
    {
        favorites.clear();
        if (favoritesFile.existsAsFile())
        {
            juce::StringArray lines;
            favoritesFile.readLines(lines);
            for (const auto& line : lines)
            {
                const auto t = line.trim();
                if (t.isNotEmpty())
                    favorites.insert(t.toStdString());
            }
        }
    }

    void saveFavorites()
    {
        juce::String content;
        for (const auto& f : favorites)
            content += juce::String(f) + "\n";
        favoritesFile.replaceWithText(content);
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userPresetDir;
    juce::File favoritesFile;
    std::set<std::string> favorites;
};
