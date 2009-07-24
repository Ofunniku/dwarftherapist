/*
Dwarf Therapist
Copyright (c) 2009 Trey Stout (chmod)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include <QVector>
#include "dwarf.h"
#include "dfinstance.h"
#include "skill.h"
#include "labor.h"
#include "defines.h"
#include "gamedatareader.h"
#include "customprofession.h"
#include "memorylayout.h"

Dwarf::Dwarf(DFInstance *df, int address, QObject *parent)
	: QObject(parent)
	, m_df(df)
	, m_address(address)
	, m_labors(new uchar[102])
	, m_pending_labors(new uchar[102])
{
	refresh_data();
}

void Dwarf::refresh_data() {
	MemoryLayout *mem = m_df->memory_layout();
	uint bytes_read = 0;

	m_first_name = m_df->read_string(m_address + mem->dwarf_offset("first_name"));
	if (m_first_name.size() > 1)
		m_first_name[0] = m_first_name[0].toUpper();
	
	m_id = m_df->read_int32(m_address + mem->dwarf_offset("id"), bytes_read);
	m_nick_name = m_df->read_string(m_address + mem->dwarf_offset("nick_name"));
	m_pending_nick_name = m_nick_name;
    m_last_name = read_last_name(m_address + mem->dwarf_offset("last_name"));
	m_custom_profession = m_df->read_string(m_address + mem->dwarf_offset("custom_profession"));
	m_pending_custom_profession = m_df->read_string(m_address + mem->dwarf_offset("custom_profession"));
	m_race_id = m_df->read_int32(m_address + mem->dwarf_offset("race"), bytes_read);
    m_skills = read_skills(m_address + mem->dwarf_offset("skills"));
	m_profession = read_professtion(m_address + mem->dwarf_offset("profession"));
	m_strength = m_df->read_int32(m_address + mem->dwarf_offset("strength"), bytes_read);
	m_toughness = m_df->read_int32(m_address + mem->dwarf_offset("toughness"), bytes_read);
	m_agility = m_df->read_int32(m_address + mem->dwarf_offset("agility"), bytes_read);
	read_labors(m_address + mem->dwarf_offset("labors"));
}

Dwarf::~Dwarf() {
	delete[] m_labors;
	delete[] m_pending_labors;
}

Dwarf *Dwarf::get_dwarf(DFInstance *df, int address) {
	MemoryLayout *mem = df->memory_layout();
	TRACE << "attempting to load dwarf at" << address << "using memory layout" << mem->game_version(); 

	uint bytes_read = 0;
	int dwarf_race_index = df->memory_layout()->address("dwarf_race_index");
	int dwarf_race_id = df->read_int32(dwarf_race_index + df->get_memory_correction(), bytes_read);
	TRACE << "Dwarf Race ID is" << dwarf_race_id;
	
	if ((df->read_int32(address + mem->dwarf_offset("flags1"), bytes_read) & mem->flags("flags1.invalidate")) > 0) {
		return 0;
	}
	if ((df->read_int32(address + mem->dwarf_offset("flags2"), bytes_read) & mem->flags("flags2.invalidate")) > 0) {
		return 0;
	}
	if ((df->read_int32(address + mem->dwarf_offset("race"), bytes_read)) != dwarf_race_id) {
		return 0;
	}
	return new Dwarf(df, address, df);
}

QString Dwarf::nice_name() {
	if (m_pending_nick_name.isEmpty()) {
		return QString("%1 %2").arg(m_first_name, m_last_name);
	} else {
		return QString("\"%1\" %2 ").arg(m_pending_nick_name, m_last_name);
	}
}

QString Dwarf::read_last_name(int address) {
	MemoryLayout *mem = m_df->memory_layout();
    uint bytes_read = 0;
    //int actual_lang_table = m_df->read_int32(gdr->get_address("language_vector") + m_df->get_memory_correction() + 4, bytes_read);
    int translations_ptr = m_df->read_int32(mem->address("translation_vector") + m_df->get_memory_correction() + 4, bytes_read);
    int translation_ptr = m_df->read_int32(translations_ptr, bytes_read);
    int actual_dwarf_translation_table = m_df->read_int32(translation_ptr + mem->offset("word_table"), bytes_read);

    QString out;

	for (int i = 0; i < 7; i++) {
        int word = m_df->read_int32(address + i * 4, bytes_read);
        if(bytes_read == 0 || word == -1)
			break;
        //Q_ASSERT(word < 10000);
        int addr = m_df->read_int32(actual_dwarf_translation_table + word * 4, bytes_read);
        out += m_df->read_string(addr);
	}
    if (out.size() > 1) {
        out[0] = out[0].toUpper();
    }
    return out;
}


QVector<Skill> Dwarf::read_skills(int address) {
    QVector<Skill> skills(0);
    uint bytes_read = 0;
	foreach(int addr, m_df->enumerate_vector(address)) {
        short type = m_df->read_short(addr, bytes_read);
        ushort experience = m_df->read_ushort(addr + 2, bytes_read);//BUG: this is wrong
        short rating = m_df->read_short(addr + 4, bytes_read);
		Skill s(type, experience, rating);
        skills.append(s);
    }
	return skills;
}

short Dwarf::get_rating_for_skill(int labor_id) {
	GameDataReader *gdr = GameDataReader::ptr();
	Labor *l = gdr->get_labor(labor_id);
	if (l->skill_id == -1) { // not found
		return 0;
	}
	foreach(Skill s, m_skills) {
		if (s.id() == l->skill_id) {
			return s.rating();
		}
	}
	return 0;
}

QString Dwarf::read_professtion(int address) {
	if (!m_custom_profession.isEmpty()) {
		return m_custom_profession; 
	}
	
	char buffer[1];
	m_df->read_raw(address, 1, &buffer[0]);
	return GameDataReader::ptr()->get_profession_name((int)buffer[0]);
}

void Dwarf::read_labors(int address) {
	//uchar buf[400];
	//memset(buf, 0, 400);
	//int bytes_read = m_df->read_raw(address, 102, &buf);
	memset(m_labors, 0, 102);
	memset(m_pending_labors, 0, 102);
	m_df->read_raw(address, 102, m_labors);
	m_df->read_raw(address, 102, m_pending_labors);
}

QVector<int> Dwarf::get_dirty_labors() {
	QVector<int> labors;
	for (int i = 0; i < 102; ++i) {
		if (is_labor_state_dirty(i))
			labors.append(i);
	}
	return labors;
}

bool Dwarf::toggle_labor(int labor_id) {
	m_pending_labors[labor_id] = m_pending_labors[labor_id] ? 0 : 1;
	//TODO: if "write_immediate"
	/*
	GameDataReader *gdr = GameDataReader::ptr();
	int bytes_written = m_df->write_raw(m_address + gdr->get_dwarf_offset("labors"), 102, m_labors);
	return bytes_written == 102;
	*/
	return true;
}

void Dwarf::set_labor(int labor_id, bool enabled) {
	m_pending_labors[labor_id] = enabled ? 1 : 0;
}

int Dwarf::pending_changes() {
	int cnt = get_dirty_labors().size();
	if (m_nick_name != m_pending_nick_name)
		cnt++;
	if (m_custom_profession != m_pending_custom_profession)
		cnt++;
	return cnt;
}

void Dwarf::clear_pending() {
	refresh_data();
}

void Dwarf::commit_pending() {
	MemoryLayout *mem = m_df->memory_layout();
	int addr = m_address + mem->dwarf_offset("labors");
	m_df->write_raw(addr, 102, m_pending_labors);
	if (m_pending_nick_name != m_nick_name)
		m_df->write_string(m_address + mem->dwarf_offset("nick_name"), m_pending_nick_name);
	if (m_pending_custom_profession != m_custom_profession)
		m_df->write_string(m_address + mem->dwarf_offset("custom_profession"), m_pending_custom_profession);
	refresh_data();
}

int Dwarf::apply_custom_profession(CustomProfession *cp) {
	memset(m_pending_labors, 0, 102); // clear all labors
	foreach(int labor_id, cp->get_enabled_labors()) {
		set_labor(labor_id, true);
	}
	m_pending_custom_profession = cp->get_name();
	return get_dirty_labors().size();
}

QTreeWidgetItem *Dwarf::get_pending_changes_tree() {
	QVector<int> labors = get_dirty_labors();
	QTreeWidgetItem *d_item = new QTreeWidgetItem;
	d_item->setText(0, nice_name() + "(" + QString::number(labors.size()) + ")");
	d_item->setData(0, Qt::UserRole, id());
	if (m_pending_nick_name != m_nick_name) {
		QTreeWidgetItem *i = new QTreeWidgetItem(d_item);
		i->setText(0, "nickname change to " + m_pending_nick_name);
		i->setIcon(0, QIcon(":img/book_edit.png"));
		i->setData(0, Qt::UserRole, id());
	}
	if (m_pending_custom_profession != m_custom_profession) {
		QTreeWidgetItem *i = new QTreeWidgetItem(d_item);
		i->setText(0, "profession change to " + m_pending_custom_profession);
		i->setIcon(0, QIcon(":img/book_edit.png"));
		i->setData(0, Qt::UserRole, id());
	}
	foreach(int labor_id, labors) {
		Labor *l = GameDataReader::ptr()->get_labor(labor_id);
		QTreeWidgetItem *i = new QTreeWidgetItem(d_item);
		i->setText(0, l->name);
		if (is_labor_enabled(labor_id)) {
			i->setIcon(0, QIcon(":img/add.png"));
		} else {
			i->setIcon(0, QIcon(":img/delete.png"));
		}
		i->setData(0, Qt::UserRole, id());
	}
	return d_item;
}