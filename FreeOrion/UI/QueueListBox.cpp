#include "QueueListBox.h"

#include <GG/DrawUtil.h>
#include <GG/WndEvent.h>

#include <boost/cast.hpp>

////////////////////////////////////////////////////////////
// PromptRow
////////////////////////////////////////////////////////////
struct PromptRow : GG::ListBox::Row {
    PromptRow(GG::X w, std::string prompt_str) {
        Resize(GG::Pt(w, GG::Y(3*ClientUI::Pts() + 15)));

        GG::TextControl* prompt = new GG::TextControl(GG::X(2), GG::Y(2), Width() - 10, Height(), prompt_str, ClientUI::GetFont(),
                                                      GG::LightColor(ClientUI::TextColor()),
                                                      GG::FORMAT_TOP | GG::FORMAT_LEFT | GG::FORMAT_LINEWRAP | GG::FORMAT_WORDBREAK);
        prompt->ClipText(true);
        Resize(prompt->Size());
        push_back(prompt);
    }
};


////////////////////////////////////////////////////////////
// QueueListBox
////////////////////////////////////////////////////////////
QueueListBox::QueueListBox(GG::X x, GG::Y y, GG::X w, GG::Y h, const std::string& drop_type_str, const std::string& prompt_str) :
    CUIListBox(x, y, w, h),
    m_drop_point(end()),
    m_show_drop_point(false),
    m_enabled(true),
    m_showing_prompt(true),
    m_prompt_str(prompt_str)
{
    AllowDropType(drop_type_str);
    
    GG::Connect(BeforeInsertSignal, &QueueListBox::EnsurePromptHiddenSlot, this);
    GG::Connect(AfterEraseSignal, &QueueListBox::ShowPromptConditionallySlot, this);
    GG::Connect(ClearedSignal, &QueueListBox::ShowPromptSlot, this);

    Insert(new PromptRow(w, m_prompt_str));
}

void QueueListBox::DropsAcceptable(DropsAcceptableIter first,
                                   DropsAcceptableIter last,
                                   const GG::Pt& pt) const
{
    assert(std::distance(first, last) == 1);
    for (DropsAcceptableIter it = first; it != last; ++it) {
        it->second = m_enabled &&
            AllowedDropTypes().find(it->first->DragDropDataType()) != AllowedDropTypes().end();
    }
}

void QueueListBox::AcceptDrops(const std::vector<GG::Wnd*>& wnds, const GG::Pt& pt) {
    assert(wnds.size() == 1);
    assert(AllowedDropTypes().find((*wnds.begin())->DragDropDataType()) != AllowedDropTypes().end());
    GG::ListBox::Row* row = boost::polymorphic_downcast<GG::ListBox::Row*>(*wnds.begin());
    assert(std::find(begin(), end(), row) != end());
    iterator it = RowUnderPt(pt);
    QueueItemMoved(row, std::distance(begin(), it));
}

void QueueListBox::Render() {
    ListBox::Render();
    // render drop point line
    if (m_show_drop_point && m_enabled) {
        GG::ListBox::Row* row = *(m_drop_point == end() ? --end() : m_drop_point);
        if (!row)
            return;
        GG::Pt ul = row->UpperLeft(), lr = row->LowerRight();
        if (m_drop_point == end())
            ul.y = lr.y;
        if (!row->empty()) {
            GG::Control* panel = (*row)[0];
            ul.x = panel->UpperLeft().x;
            lr.x = panel->LowerRight().x;
        }
        GG::FlatRectangle(GG::Pt(ul.x, ul.y - 1), GG::Pt(lr.x, ul.y), GG::CLR_ZERO, GG::CLR_WHITE, 1);
    }
}

void QueueListBox::DragDropEnter(const GG::Pt& pt, const std::map<GG::Wnd*, GG::Pt>& drag_drop_wnds,
                                 GG::Flags<GG::ModKey> mod_keys)
{ DragDropHere(pt, drag_drop_wnds, mod_keys); }

void QueueListBox::DragDropHere(const GG::Pt& pt, const std::map<GG::Wnd*, GG::Pt>& drag_drop_wnds,
                                GG::Flags<GG::ModKey> mod_keys)
{
    CUIListBox::DragDropHere(pt, drag_drop_wnds, mod_keys);
    if (drag_drop_wnds.size() == 1 &&
        AllowedDropTypes().find(drag_drop_wnds.begin()->first->DragDropDataType()) !=
        AllowedDropTypes().end()) {
        m_drop_point = RowUnderPt(pt);
        m_show_drop_point = true;
    } else {
        m_drop_point = end();
        m_show_drop_point = false;
    }
}

void QueueListBox::DragDropLeave() {
    m_drop_point = end();
    m_show_drop_point = false;
}

void QueueListBox::EnableOrderIssuing(bool enable/* = true*/)
{ m_enabled = enable; }

void QueueListBox::Clear() {
    CUIListBox::Clear();
    DragDropLeave();
}

void QueueListBox::EnsurePromptHiddenSlot(iterator it) {
    if (m_showing_prompt) {
        Erase(begin(), false, false); // if the prompt is shown, it must be the only row in the ListBox
        m_showing_prompt = false;
    }
}

void QueueListBox::ShowPromptSlot() {
    Insert(new PromptRow(Width() - 4, m_prompt_str), begin(), false, false);
    m_showing_prompt = true;
}

void QueueListBox::ShowPromptConditionallySlot(iterator it) {
    if (begin() == end()) {
        Insert(new PromptRow(Width() - 4, m_prompt_str), begin(), false, false);
        m_showing_prompt = true;
    }
}

bool QueueListBox::DisplayingValidQueueItems() {
    return !m_showing_prompt;
}