#pragma once

#include "pch.h"
#include <string>

class database_guard final {
public:
	database_guard(database &db, std::string const &config_file)
		: m_db(db), m_opened(false) {
		task::initialize(task::normal_stack);
		m_opened = m_db.open(config_file.c_str());
	}
	~database_guard() { m_db.close(); }

	operator bool() const { return m_opened; }
private:
	database &m_db;
	bool m_opened;
};
