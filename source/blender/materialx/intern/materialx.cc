/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Esteban Tovagliari, Cedric Paille, Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../MTLX_api.h"

#include "../tinyxml2/tinyxml2.h"

void MTLX_export(const char *filename)
{
	tinyxml2::XMLDocument doc;
	doc.InsertFirstChild(doc.NewDeclaration());

	tinyxml2::XMLElement *root = doc.NewElement("materialx");
	root->SetAttribute("version", "1.32");
	root->SetAttribute("cms", "ocio");

	doc.InsertEndChild(root);

	tinyxml2::XMLError eResult = doc.SaveFile(filename);

	if (eResult != tinyxml2::XML_SUCCESS) {
		printf("Error: %i\n", eResult);
	}
}
