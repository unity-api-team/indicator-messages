/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Ted Gould <ted@canonical.com>
 */

#include <gtest/gtest.h>
#include <gio/gio.h>

#include "indicator-fixture.h"

class IndicatorTest : public IndicatorFixture
{
protected:
	IndicatorTest (void) :
		IndicatorFixture(INDICATOR_MESSAGES_SERVICE_BINARY, "com.canonical.indicator.messages")
	{
	}

	virtual void SetUp() override
	{
		g_setenv("GSETTINGS_SCHEMA_DIR", SCHEMA_DIR, TRUE);
		g_setenv("GSETTINGS_BACKEND", "memory", TRUE);

		IndicatorFixture::SetUp();
	}

	virtual void TearDown() override
	{
		IndicatorFixture::TearDown();
	}

};


TEST_F(IndicatorTest, PhoneMenu) {
	setMenu("/com/canonical/indicator/messages/phone");

}

