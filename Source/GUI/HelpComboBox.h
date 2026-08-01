// ==========================================
// File: HelpComboBox.h
// 「開いたメニューの各項目にマウスを乗せると、その項目の説明が
//   下部インフォバーに出る」コンボボックス。
//
//  JUCE 標準の ComboBox はポップアップを内部で組み立ててしまうため、
//  showPopup() を差し替えて PopupMenu::CustomComponent で作り直す。
//  CustomComponent は setHighlighted() でハイライト状態を通知してくれるので、
//  そこで説明文を MenuHelpBus へ流す。
//
//  項目別の説明 (setItemHelp) を渡していない場合は、コンボ自身の
//  setTooltip() の文言にフォールバックする。
//  → ポップアップを開いている間もインフォバーが空にならない。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

// ポップアップメニューの項目ヘルプを PluginEditor へ渡す共有バス。
//  同時に開けるポップアップはプロセス内で1つだけなので、静的1本で足りる。
//  複数インスタンスでの誤表示を防ぐため、どのコンボが出したかも持たせる。
struct MenuHelpBus
{
    static juce::Component*& owner() { static juce::Component* c = nullptr; return c; }
    static juce::String&     text()  { static juce::String t;               return t; }

    static void set(juce::Component* o, const juce::String& t)
    {
        owner() = o;
        text()  = t;
    }
    static void clear(juce::Component* o)
    {
        if (owner() == o)
        {
            owner() = nullptr;
            text().clear();
        }
    }
};

class HelpComboBox : public juce::ComboBox
{
public:
    HelpComboBox() = default;
    ~HelpComboBox() override { MenuHelpBus::clear(this); }

    // addItem した順と同じ並びで項目別の説明文を渡す。
    // 省略した項目・空文字の項目は setTooltip() の文言が使われる。
    void setItemHelp(const juce::StringArray& help) { mHelp = help; }

    void showPopup() override
    {
        if (getNumItems() <= 0)
        {
            hidePopup();
            return;
        }

        juce::PopupMenu menu;
        menu.setLookAndFeel(&getLookAndFeel());

        const int selectedId = getSelectedId();
        for (int i = 0; i < getNumItems(); ++i)
        {
            const int  id   = getItemId(i);
            const bool tick = (id == selectedId);
            juce::String help = (i < mHelp.size() && mHelp[i].isNotEmpty())
                                  ? mHelp[i] : getTooltip();

            menu.addCustomItem(id,
                std::make_unique<HelpItem>(*this, getItemText(i), help, tick),
                nullptr);
        }

        juce::Component::SafePointer<HelpComboBox> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options()
                             .withTargetComponent(this)
                             .withMinimumWidth(getWidth())
                             .withStandardItemHeight(22),
            [safe](int result) mutable
            {
                if (auto* box = safe.getComponent())
                {
                    MenuHelpBus::clear(box);
                    box->hidePopup();                 // ComboBox の menuActive を戻す
                    if (result != 0)
                        box->setSelectedId(result, juce::sendNotificationSync);
                }
            });
    }

private:
    // メニュー1項目ぶんの描画 + ハイライト通知
    class HelpItem : public juce::PopupMenu::CustomComponent
    {
    public:
        // owner は juce::Component& で受ける。囲っている HelpComboBox は
        // この時点ではまだ不完全型なので、SafePointer<HelpComboBox> にすると
        // コンパイラによっては実体化で問題が出る。
        HelpItem(juce::Component& ownerBox, juce::String itemText,
                 juce::String helpText, bool ticked)
            : juce::PopupMenu::CustomComponent(true),   // クリックで自動的に確定
              mOwner(&ownerBox),
              mText(std::move(itemText)),
              mHelp(std::move(helpText)),
              mTicked(ticked)
        {
        }

        void getIdealSize(int& idealWidth, int& idealHeight) override
        {
            // 文字幅の計測APIは JUCE のバージョン差が大きいので使わない。
            // コンボ本体の幅に合わせておけば見た目は揃う。
            auto* box = mOwner.getComponent();
            const int ownerW = (box != nullptr) ? box->getWidth() : 0;
            idealWidth  = juce::jmax(150, ownerW);
            idealHeight = 22;
        }

        // ハイライトが移動したタイミングで説明文を流す。
        //  ※ override を付けていないのは意図的。JUCE のバージョンによって
        //    CustomComponent::setHighlighted が virtual でないことがあるため。
        //    仮想でなければここは呼ばれないが、その場合は paint() 側が拾う。
        void setHighlighted(bool shouldBeHighlighted)
        {
            juce::PopupMenu::CustomComponent::setHighlighted(shouldBeHighlighted);
            if (shouldBeHighlighted)
                MenuHelpBus::set(mOwner.getComponent(), mHelp);
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            // ハイライト変更時は必ず再描画されるので、ここでも保険として流しておく
            if (isItemHighlighted())
                MenuHelpBus::set(mOwner.getComponent(), mHelp);

            auto r = getLocalBounds();

            if (isItemHighlighted())
            {
                g.setColour(SpectraColors::mint.withAlpha(0.22f));
                g.fillRect(r);
            }

            if (mTicked)
            {
                g.setColour(SpectraColors::mint);
                g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
                g.drawText("*", r.removeFromLeft(18), juce::Justification::centred);
            }
            else
            {
                r.removeFromLeft(18);
            }

            g.setColour(isItemHighlighted() ? SpectraColors::text : SpectraColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(13.0f)));
            g.drawText(mText, r.reduced(4, 0), juce::Justification::centredLeft, true);
        }

    private:
        juce::Component::SafePointer<juce::Component> mOwner;
        juce::String mText, mHelp;
        bool mTicked = false;
    };

    juce::StringArray mHelp;
};
