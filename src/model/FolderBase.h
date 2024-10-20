/*======================================================================================================================
 * model/FolderBase.h is part of Brewken, and is copyright the following authors 2024:
 *   • Matt Young <mfsy@yahoo.com>
 *
 * Brewken is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Brewken is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 =====================================================================================================================*/
#ifndef MODEL_FOLDERBASE_H
#define MODEL_FOLDERBASE_H
#pragma once

#include <QString>

#include "model/NamedParameterBundle.h"
#include "utils/CuriouslyRecurringTemplateBase.h"
#include "utils/TypeLookup.h"

//======================================================================================================================
//========================================== Start of property name constants ==========================================
// See comment in model/NamedEntity.h
#define AddPropertyName(property) namespace PropertyNames::FolderBase { BtStringConst const property{#property}; }
AddPropertyName(folder)
#undef AddPropertyName
//=========================================== End of property name constants ===========================================
//======================================================================================================================

/**
 * \brief Adds a "folder" property to a class.  Typically this is used only on classes that are not dependent on others
 *        (see comments in model/NamedEntity.h).
 */
template<class Derived> class FolderBasePhantom;
template<class Derived>
class FolderBase : public CuriouslyRecurringTemplateBase<FolderBasePhantom, Derived> {

protected:
   // Note that, because this is static, it cannot be initialised inside the class definition
   static TypeLookup const typeLookup;

   //! Non-virtual equivalent of isEqualTo
   bool doIsEqualTo([[maybe_unused]] FolderBase const & other) const {
      // For the moment at least, we do not consider the fact that things are in different folders prevents them from
      // being equal.
      return true;
   }

private:
   friend Derived;
   FolderBase() :
      m_folder{""} {
      return;
   }

   FolderBase(NamedParameterBundle const & namedParameterBundle) :
      SET_REGULAR_FROM_NPB(m_folder, namedParameterBundle, PropertyNames::FolderBase::folder, "") {
      return;
   }

   FolderBase(FolderBase const & other) :
      m_folder{other.m_folder} {
      return;
   }

   ~FolderBase() = default;

protected:
   QString const & getFolder() const {
      return this->m_folder;
   }

   void doSetFolder(QString const & val) {
      this->derived().setAndNotify(PropertyNames::FolderBase::folder, this->m_folder, val);
      return;
   }

protected:
   QString m_folder;
};

template<class Derived>
TypeLookup const FolderBase<Derived>::typeLookup {
   "FolderBase",
   {
      //
      // See comment in model/IngredientAmount.h for why we can't use the PROPERTY_TYPE_LOOKUP_ENTRY or
      // PROPERTY_TYPE_LOOKUP_ENTRY_NO_MV macros here.
      //
      {&PropertyNames::FolderBase::folder,
       TypeInfo::construct<decltype(FolderBase<Derived>::m_folder)>(
          PropertyNames::FolderBase::folder,
          TypeLookupOf<decltype(FolderBase<Derived>::m_folder)>::value
       )}
   },
   // Parent class lookup: none as we are at the top of this branch of the inheritance tree
   {}
};

/**
 * \brief Concrete derived classes should (either directly or via inclusion in an intermediate class's equivalent macro)
 *        include this in their header file, right after Q_OBJECT.  Concrete derived classes also need to include the
 *        following block (see comment in model/StepBase.h for why):
 *
 *           // See model/FolderBase.h for info, getters and setters for these properties
 *           Q_PROPERTY(QString folder        READ folder        WRITE setFolder     )
 *
 *        Comments for these properties:
 *
 *           \c folder : Currently this is the name of the folder, but ultimately we'd like to make it the \c Folder
 *                       object itself.
 *
 *        Note we have to be careful about comment formats in macro definitions
 */
#define FOLDER_BASE_DECL(Derived) \
   /* This allows FolderBase to call protected and private members of Derived. */           \
   friend class FolderBase<Derived>;                                                        \
                                                                                            \
   public:                                                                                  \
      /*=========================== FB "GETTER" MEMBER FUNCTIONS ===========================*/ \
      virtual QString const & folder() const;                                                  \
      /*=========================== FB "SETTER" MEMBER FUNCTIONS ===========================*/ \
      virtual void setFolder(QString const & val);                                             \

/**
 * \brief Derived classes should include this in their .cpp file
 *
 *        Note we have to be careful about comment formats in macro definitions.
 */
#define FOLDER_BASE_COMMON_CODE(Derived) \
   /*====================================== FB "GETTER" MEMBER FUNCTIONS ======================================*/ \
   QString const & Derived::folder() const { return this->getFolder(); }                                          \
   /*====================================== FB "SETTER" MEMBER FUNCTIONS ======================================*/ \
   void Derived::setFolder(QString const & val) { this->doSetFolder(val); return; }                               \

#endif
