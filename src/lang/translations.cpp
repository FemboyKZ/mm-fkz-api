#include "translations.h"

#include "game/cvarquery.h"

mmu::Translations g_FKZTranslations;

std::string FKZ_Translate(int slot, const char *phrase)
{
	const std::string lang = g_FKZTranslations.MapClientLanguage(mmu::cvarquery::GetClientLanguage(slot));
	return g_FKZTranslations.Translate(lang, phrase ? phrase : "");
}
