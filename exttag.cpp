#include "exttag.h"
#include <boost/preprocessor.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <windows.h>
#include <propkey.h>

#include <asftag.h>
#include <apetag.h>
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <xiphcomment.h>

using namespace TagLib;

#define CLASSES (APE::Tag)(ASF::Tag)(ID3v1::Tag)(ID3v2::Tag)(Ogg::XiphComment)

#define UPCAST_CALL_N_RETURN(r, data, elem) if (const elem *t = dynamic_cast<const elem *>(tag)) return data(t);

const String emptyString;

const String &readTXXXString(const ID3v2::Tag *tag, const String &name)
{
	const ID3v2::FrameList &fl = tag->frameListMap()["TXXX"];
	for (ID3v2::FrameList::ConstIterator it = fl.begin(); it != fl.end(); ++it)
	{
		const ID3v2::TextIdentificationFrame *tif = dynamic_cast<const ID3v2::TextIdentificationFrame *>(*it);
		const StringList &sl = tif->fieldList();
		if (sl.size() >= 2)
			if (sl[0].upper() == name.upper())
				return sl[1];
	}

	return emptyString;
}


unsigned char normaliseRating(int rat)
{
	if (rat > 0)
		switch (rat)
		{
			case 1: return RATING_ONE_STAR_SET;
			case 2: return RATING_TWO_STARS_SET;
			case 3: return RATING_THREE_STARS_SET;
			case 4: return RATING_FOUR_STARS_SET;
			case 5: return RATING_FIVE_STARS_SET;
			default:
				{
					if (rat < 100)
						return rat;
					if (rat < 255)
						return static_cast<unsigned char>(rat*100/255.f);
				}
		}
	return RATING_UNRATED_SET;
}

unsigned char readRating(const APE::Tag *tag)
{
	const StringList& lst = tag->itemListMap()[L"rating"].values();
	if (lst.size())
		return normaliseRating(lst[0].toInt());
	return RATING_UNRATED_SET;
}

// XXX Unimplemented.
unsigned char readRating(const ASF::Tag *tag)
{
	return RATING_UNRATED_SET;
}

// No way to store a rating in a v1 tag.
unsigned char readRating(const ID3v1::Tag *tag)
{
	return RATING_UNRATED_SET;
}

unsigned char readRating(const ID3v2::Tag *tag)
{
	// First attempt to read from 4.17 Popularimeter. (http://www.id3.org/id3v2.4.0-frames)
	//    <Header for 'Popularimeter', ID: "POPM">
    // Email to user   <text string> $00
    // Rating          $xx
    // Counter         $xx xx xx xx (xx ...)
	// The rating is 1-255 where 1 is worst and 255 is best. 0 is unknown.

	const ID3v2::FrameList &fl = tag->frameListMap()["POPM"];
	for (ID3v2::FrameList::ConstIterator it = fl.begin(); it != fl.end(); ++it)
	{
		const ByteVector &s = (*it)->toString().data(String::Latin1);

		// Locate the null byte.
		ByteVector::ConstIterator sp = std::find(s.begin(), s.end(), 0);
		// If we found it, and the string continues, at least another character, 
		//  the continued character is the rating byte.
		if (sp != s.end() && ++sp != s.end())
			return static_cast<unsigned char>(
				static_cast<unsigned char>(*sp)*100/255.f);
	}

	return normaliseRating(readTXXXString(tag, "rating").toInt());
}

unsigned char readRating(const Ogg::XiphComment *tag)
{
	const StringList &sl = tag->fieldListMap()["RATING"];
	for (StringList::ConstIterator it = sl.begin(); it != sl.end(); ++it)
		if (int i = it->toInt())
			return normaliseRating(i);
	return RATING_UNRATED_SET;
}

unsigned char rating(const TagLib::Tag *tag)
{
	BOOST_PP_SEQ_FOR_EACH(UPCAST_CALL_N_RETURN, readRating, CLASSES)

	return RATING_UNRATED_SET;
}
