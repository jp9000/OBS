/********************************************************************************
 Copyright (C) 2001-2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "XT.h"




struct StringLookupNode
{
    String str;
    List<StringLookupNode*> subNodes;
    LocaleStringItem *leaf;

    inline ~StringLookupNode()
    {
        for(unsigned int i=0; i<subNodes.Num(); i++)
            delete subNodes[i];
    }

    inline StringLookupNode* FindSubNodeByChar(TCHAR ch)
    {
        for(unsigned int i=0; i<subNodes.Num(); i++)
        {
            StringLookupNode *node = subNodes[i];
            if(node->str.IsValid() && node->str[0] == ch)
                return subNodes[i];
        }

        return NULL;
    }

    inline UINT FindSubNodeID(CTSTR lpLookup)
    {
        for(unsigned int i=0; i<subNodes.Num(); i++)
        {
            StringLookupNode *node = subNodes[i];
            if(scmpi_n(node->str, lpLookup, node->str.Length()) == 0)
                return i;
        }

        return INVALID;
    }

    inline StringLookupNode* FindSubNode(CTSTR lpLookup)
    {
        for(unsigned int i=0; i<subNodes.Num(); i++)
        {
            StringLookupNode *node = subNodes[i];
            if(scmpi_n(node->str, lpLookup, node->str.Length()) == 0)
                return subNodes[i];
        }

        return NULL;
    }
};


LocaleStringLookup *locale=NULL;



LocaleStringLookup::LocaleStringLookup()
{
    top = new StringLookupNode;
}

LocaleStringLookup::~LocaleStringLookup()
{
    cache.Clear();
    delete top;
}


void LocaleStringLookup::AddLookup(CTSTR lookupVal, LocaleStringItem *item, StringLookupNode *node)
{
    if(!node) node = top;

    if(!lookupVal)
        return;

    if(!*lookupVal)
    {
        delete node->leaf;
        node->leaf = item;
        return;
    }

    StringLookupNode *child = node->FindSubNodeByChar(*lookupVal);

    if(child)
    {
        UINT len;

        for(len=0; len<child->str.Length(); len++)
        {
            TCHAR val1 = child->str[len],
                  val2 = lookupVal[len];

            if((val1 >= 'A') && (val1 <= 'Z'))
                val1 += 0x20;
            if((val2 >= 'A') && (val2 <= 'Z'))
                val2 += 0x20;

            if(val1 != val2)
                break;
        }

        if(len == child->str.Length())
            return AddLookup(lookupVal+len, item, child);
        else
        {
            StringLookupNode *childSplit = new StringLookupNode;
            childSplit->str = child->str.Array()+len;
            childSplit->leaf = child->leaf;
            childSplit->subNodes.TransferFrom(child->subNodes);

            child->leaf = NULL;
            child->str.SetLength(len);

            child->subNodes << childSplit;

            if(lookupVal[len] != 0)
            {
                StringLookupNode *newNode = new StringLookupNode;
                newNode->leaf = item;
                newNode->str  = lookupVal+len;

                child->subNodes << newNode;
            }
            else
                child->leaf = item;
        }
    }
    else
    {
        StringLookupNode *newNode = new StringLookupNode;
        newNode->leaf = item;
        newNode->str  = lookupVal;

        node->subNodes << newNode;
    }
}

void LocaleStringLookup::RemoveLookup(CTSTR lookupVal, StringLookupNode *node)
{
    if(!node) node = top;

    UINT childID = node->FindSubNodeID(lookupVal);
    if(childID == INVALID)
        return;

    StringLookupNode *child = node->subNodes[childID];

    lookupVal += child->str.Length();
    TCHAR ch = *lookupVal;
    if(ch)
        RemoveLookup(lookupVal, child);

    if(!ch)
    {
        if(!child->subNodes.Num())
        {
            if(child->leaf)
            {
                cache.RemoveItem(child->leaf);
                delete child->leaf;
            }

            node->subNodes.Remove(childID);
            delete child;
        }
        else
        {
            if(child->leaf)
            {
                cache.RemoveItem(child->leaf);
                delete child->leaf;
            }

            child->leaf = NULL;

            if(child->subNodes.Num() == 1)
            {
                StringLookupNode *subNode = child->subNodes[0];

                child->str += subNode->str;
                child->leaf = subNode->leaf;
                child->subNodes.CopyList(subNode->subNodes);
                subNode->subNodes.Clear();
                delete subNode;
            }
        }
    }
    else if(!child->subNodes.Num() && !child->leaf)
    {
        node->subNodes.Remove(childID);
        delete child;
    }

    //if not a leaf node and only have one child node, then merge with child node
    if(!node->leaf && node->subNodes.Num() == 1 && node != top)
    {
        StringLookupNode *subNode = node->subNodes[0];

        node->str += subNode->str;
        node->leaf = subNode->leaf;
        node->subNodes.CopyList(subNode->subNodes);
        subNode->subNodes.Clear();
        delete subNode;
    }
}

//ugh yet more string parsing, you think you escape it for one minute and then bam!  you discover yet more string parsing code needs to be written
BOOL LocaleStringLookup::LoadStringFile(CTSTR lpFile, bool bClear)
{
    if(bClear)
    {
        cache.Clear();
        delete top;
        top = new StringLookupNode;
    }
    else if(!top)
        top = new StringLookupNode;

    //------------------------

    XFile file;

    if(!file.Open(lpFile, XFILE_READ, XFILE_OPENEXISTING))
        return FALSE;

    String fileString;
    file.ReadFileToString(fileString);
    file.Close();

    if(fileString.IsEmpty())
        return FALSE;

    //------------------------

    fileString.FindReplace(TEXT("\r"), TEXT(" "));

    TSTR lpTemp = fileString.Array()-1;
    TSTR lpNextLine;

    do
    {
        ++lpTemp;
        lpNextLine = schr(lpTemp, '\n');

        while(*lpTemp == ' ' || *lpTemp == L'　' || *lpTemp == '\t')
            ++lpTemp;

        if(!*lpTemp || *lpTemp == '\n') continue;

        if(lpNextLine) *lpNextLine = 0;

        //----------

        TSTR lpValueStart = lpTemp;
        while(*lpValueStart && *lpValueStart != '=')
            ++lpValueStart;

        String lookupVal, strVal;

        TCHAR prevChar = *lpValueStart;
        *lpValueStart = 0;
        lookupVal = lpTemp;
        *lpValueStart = prevChar;
        lookupVal.KillSpaces();

        String value = ++lpValueStart;
        value.KillSpaces();
        if(value.IsValid() && value[0] == '"')
        {
            value = String::RepresentationToString(value);
            strVal = value;
        }
        else
            strVal = value;

        if(lookupVal.IsValid())
            AddLookupString(lookupVal, strVal);

        //----------

        if(lpNextLine) *lpNextLine = '\n';
    }while(lpTemp = lpNextLine);

    //------------------------

    return TRUE;
}


StringLookupNode *LocaleStringLookup::FindNode(CTSTR lookupVal, StringLookupNode *node) const
{
    if(!node) node = top;

    StringLookupNode *child = node->FindSubNode(lookupVal);
    if(child)
    {
        lookupVal += child->str.Length();
        TCHAR ch = *lookupVal;
        if(ch)
            return FindNode(lookupVal, child);

        if(child->leaf)
            return child;
    }

    return NULL;
}

void LocaleStringLookup::AddLookupString(CTSTR lookupVal, CTSTR lpVal)
{
    assert(lookupVal && *lookupVal);

    if(!lookupVal || !*lookupVal)
        return;

    StringLookupNode *child = FindNode(lookupVal);
    if(child)
        child->leaf->strValue = lpVal;
    else
    {
        LocaleStringItem *item = new LocaleStringItem;
        item->lookup = lookupVal;
        item->strValue = lpVal;
        cache << item;

        AddLookup(item->lookup, item);
    }
}

CTSTR LocaleStringLookup::LookupString(CTSTR lookupVal)
{
    StringLookupNode *child = FindNode(lookupVal);
    if(!child)
        return TEXT("(string not found)");

    if(!child->leaf)
        return TEXT("(lookup error)");

    return child->leaf->strValue;
}


#ifdef UNICODE

LocaleNativeName nativeNames[] =
{
    {TEXT("aa"), TEXT("Afaraf")},
    {TEXT("ab"), TEXT("Аҧсуа")},
    {TEXT("ae"), TEXT("avesta")},
    {TEXT("af"), TEXT("Afrikaans")},
    {TEXT("ak"), TEXT("Akan")},
    {TEXT("am"), TEXT("Amharic")},
    {TEXT("an"), TEXT("aragonés")},
    {TEXT("ar"), TEXT("العربية")},
    {TEXT("as"), TEXT("অসমীয়া")},
    {TEXT("av"), TEXT("авар мацӀ, магӀарул мацӀ")},
    {TEXT("ay"), TEXT("aymar aru")},
    {TEXT("az"), TEXT("azərbaycan dili")},
    {TEXT("ba"), TEXT("башҡорт теле")},
    {TEXT("be"), TEXT("Беларуская")},
    {TEXT("bg"), TEXT("български език")},
    {TEXT("bh"), TEXT("भोजपुरी")},
    {TEXT("bi"), TEXT("Bislama")},
    {TEXT("bm"), TEXT("bamanankan")},
    {TEXT("bn"), TEXT("বাংলা")},
    {TEXT("bo"), TEXT("བོད་ཡིག")},
    {TEXT("br"), TEXT("brezhoneg")},
    {TEXT("bs"), TEXT("jezik")},
    {TEXT("ca"), TEXT("Català")},
    {TEXT("ce"), TEXT("нохчийн мотт")},
    {TEXT("ch"), TEXT("Chamoru")},
    {TEXT("co"), TEXT("corsu, lingua corsa")},
    {TEXT("cr"), TEXT("Cree")},
    {TEXT("cs"), TEXT("česky, čeština")},
    {TEXT("cu"), TEXT("ѩзыкъ словѣньскъ")},
    {TEXT("cv"), TEXT("чӑваш чӗлхи")},
    {TEXT("cy"), TEXT("Cymraeg")},
    {TEXT("da"), TEXT("dansk")},
    {TEXT("de"), TEXT("Deutsch")},
    {TEXT("dv"), TEXT("ދ ިވެހި")},
    {TEXT("dz"), TEXT("རྫོང་ཁ")},
    {TEXT("ee"), TEXT("Eʋegbe")},
    {TEXT("el"), TEXT("Ελληνικά")},
    {TEXT("en"), TEXT("English")},
    {TEXT("eo"), TEXT("Esperanto")},
    {TEXT("es"), TEXT("español, castellano")},
    {TEXT("et"), TEXT("eesti, eesti keel")},
    {TEXT("eu"), TEXT("euskara, euskera")},
    {TEXT("fa"), TEXT("فارسی")},
    {TEXT("ff"), TEXT("Fulfulde, Pulaar, Pular")},
    {TEXT("fi"), TEXT("suomi, suomen kieli")},
    {TEXT("fj"), TEXT("vosa Vakaviti")},
    {TEXT("fo"), TEXT("føroyskt")},
    {TEXT("fr"), TEXT("français, langue française")},
    {TEXT("fy"), TEXT("Frysk")},
    {TEXT("ga"), TEXT("Gaeilge")},
    {TEXT("gd"), TEXT("Gàidhlig")},
    {TEXT("gl"), TEXT("Galego")},
    {TEXT("gn"), TEXT("Avañe'ẽ")},
    {TEXT("gu"), TEXT("ગુજરાતી")},
    {TEXT("gv"), TEXT("Gaelg, Gailck")},
    {TEXT("ha"), TEXT("Hausa, هَوُسَ")},
    {TEXT("he"), TEXT("עברית")},
    {TEXT("hi"), TEXT("हिन्दी, हिंदी")},
    {TEXT("ho"), TEXT("Hiri Motu")},
    {TEXT("hr"), TEXT("hrvatski")},
    {TEXT("ht"), TEXT("Kreyòl ayisyen")},
    {TEXT("hu"), TEXT("Magyar")},
    {TEXT("hy"), TEXT("Հայերեն")},
    {TEXT("hz"), TEXT("Otjiherero")},
    {TEXT("ia"), TEXT("Interlingua")},
    {TEXT("id"), TEXT("Bahasa Indonesia")},
    {TEXT("ie"), TEXT("Interlingue")},
    {TEXT("ig"), TEXT("Igbo")},
    {TEXT("ii"), TEXT("ꆇꉙ")},
    {TEXT("ik"), TEXT("Iñupiaq, Iñupiatun")},
    {TEXT("io"), TEXT("Ido")},
    {TEXT("is"), TEXT("Íslenska")},
    {TEXT("it"), TEXT("Italiano")},
    {TEXT("iu"), TEXT("Inuktitut")},
    {TEXT("ja"), TEXT("日本語")},
    {TEXT("jv"), TEXT("basa Jawa")},
    {TEXT("ka"), TEXT("ქართული")},
    {TEXT("kg"), TEXT("KiKongo")},
    {TEXT("ki"), TEXT("Gĩkũyũ")},
    {TEXT("kj"), TEXT("Kuanyama")},
    {TEXT("kk"), TEXT("Қазақ тілі")},
    {TEXT("kl"), TEXT("kalaallisut, kalaallit oqaasii")},
    {TEXT("km"), TEXT("ភាសាខ្មែរ")},
    {TEXT("kn"), TEXT("ಕನ್ನಡ")},
    {TEXT("ko"), TEXT("한국어")},
    {TEXT("kr"), TEXT("Kanuri")},
    {TEXT("ks"), TEXT("कश्मीरी, كشميري")},
    {TEXT("ku"), TEXT("Kurdî, كوردی")},
    {TEXT("kv"), TEXT("коми кыв")},
    {TEXT("kw"), TEXT("Kernewek")},
    {TEXT("ky"), TEXT("кыргыз тили")},
    {TEXT("la"), TEXT("latine, lingua latina")},
    {TEXT("lb"), TEXT("Lëtzebuergesch")},
    {TEXT("lg"), TEXT("Luganda")},
    {TEXT("li"), TEXT("Limburgs")},
    {TEXT("ln"), TEXT("Lingála")},
    {TEXT("lo"), TEXT("Lao")},
    {TEXT("lt"), TEXT("lietuvių kalba")},
    {TEXT("lu"), TEXT("Kiluba")},
    {TEXT("lv"), TEXT("latviešu valoda")},
    {TEXT("mg"), TEXT("Malagasy fiteny")},
    {TEXT("mh"), TEXT("Kajin M̧ajeļ")},
    {TEXT("mi"), TEXT("te reo Māori")},
    {TEXT("mk"), TEXT("македонски јазик")},
    {TEXT("ml"), TEXT("മലയാളം")},
    {TEXT("mn"), TEXT("Монгол")},
    {TEXT("mr"), TEXT("मराठी")},
    {TEXT("ms"), TEXT("bahasa Melayu, بهاس ملايو")},
    {TEXT("mt"), TEXT("Malti")},
    {TEXT("my"), TEXT("Burmese")},
    {TEXT("na"), TEXT("Ekakairũ Naoero")},
    {TEXT("nb"), TEXT("Norsk bokmål")},
    {TEXT("nd"), TEXT("isiNdebele")},
    {TEXT("ne"), TEXT("नेपाली")},
    {TEXT("ng"), TEXT("Owambo")},
    {TEXT("nl"), TEXT("Nederlands, Vlaams")},
    {TEXT("nn"), TEXT("Norsk nynorsk")},
    {TEXT("no"), TEXT("Norsk")},
    {TEXT("nr"), TEXT("isiNdebele")},
    {TEXT("nv"), TEXT("Diné bizaad, Dinékʼehǰí")},
    {TEXT("ny"), TEXT("chiCheŵa, chinyanja")},
    {TEXT("oc"), TEXT("Occitan")},
    {TEXT("oj"), TEXT("Anishinaabe")},
    {TEXT("om"), TEXT("Afaan Oromoo")},
    {TEXT("or"), TEXT("ଓଡ଼ିଆ")},
    {TEXT("os"), TEXT("Ирон æвзаг")},
    {TEXT("pa"), TEXT("ਪੰਜਾਬੀ, پنجابی")},
	// reserving this region code, but not displaying it in the UI until the translation is more complete :D
	// {TEXT("pe"), TEXT("Pirate English")},
    {TEXT("pi"), TEXT("पाऴि ")},
    {TEXT("pl"), TEXT("polski")},
    {TEXT("ps"), TEXT("پښتو")},
    {TEXT("pt"), TEXT("Português")},
    {TEXT("qu"), TEXT("Runa Simi, Kichwa")},
    {TEXT("rm"), TEXT("rumantsch grischun")},
    {TEXT("rn"), TEXT("kiRundi")},
    {TEXT("ro"), TEXT("română")},
    {TEXT("ru"), TEXT("Русский язык")},
    {TEXT("rw"), TEXT("Ikinyarwanda")},
    {TEXT("sa"), TEXT("संस्कृतम्")},
    {TEXT("sc"), TEXT("sardu")},
    {TEXT("sd"), TEXT("सिन्धी, سنڌي، سندھی")},
    {TEXT("se"), TEXT("Davvisámegiella")},
    {TEXT("sg"), TEXT("yângâ tî sängö")},
    {TEXT("si"), TEXT("සිංහල")},
    {TEXT("sk"), TEXT("slovenčina")},
    {TEXT("sl"), TEXT("slovenščina")},
    {TEXT("sm"), TEXT("gagana fa'a Samoa")},
    {TEXT("sn"), TEXT("chiShona")},
    {TEXT("so"), TEXT("Soomaaliga, af Soomaali")},
    {TEXT("sq"), TEXT("Shqip")},
    {TEXT("sr"), TEXT("српски језик")},
    {TEXT("ss"), TEXT("SiSwati")},
    {TEXT("st"), TEXT("Sesotho")},
    {TEXT("su"), TEXT("Basa Sunda")},
    {TEXT("sv"), TEXT("svenska")},
    {TEXT("sw"), TEXT("Kiswahili")},
    {TEXT("ta"), TEXT("தமிழ்")},
    {TEXT("te"), TEXT("తెలుగు")},
    {TEXT("tg"), TEXT("тоҷикӣ, toğikī, تاجیکی")},
    {TEXT("th"), TEXT("ไทย")},
    {TEXT("ti"), TEXT("Tigrinya")},
    {TEXT("tk"), TEXT("Türkmen, Түркмен")},
    {TEXT("tl"), TEXT("Wikang Tagalog")},
    {TEXT("tn"), TEXT("Setswana")},
    {TEXT("to"), TEXT("faka Tonga")},
    {TEXT("tr"), TEXT("Türkçe")},
    {TEXT("ts"), TEXT("Xitsonga")},
    {TEXT("tt"), TEXT("татарча, tatarça, تاتارچا")},
    {TEXT("tw"), TEXT("正體中文")},
    {TEXT("ty"), TEXT("Reo Mā`ohi")},
    {TEXT("ug"), TEXT("ئۇيغۇرچە")},
    {TEXT("uk"), TEXT("українська")},
    {TEXT("ur"), TEXT("اردو")},
    {TEXT("uz"), TEXT("O'zbek, Ўзбек, أۇزبېك")},
    {TEXT("ve"), TEXT("Tshivenḓa")},
    {TEXT("vi"), TEXT("Tiếng Việt")},
    {TEXT("vo"), TEXT("Volapük")},
    {TEXT("wa"), TEXT("Walon")},
    {TEXT("wo"), TEXT("Wollof")},
    {TEXT("xh"), TEXT("isiXhosa")},
    {TEXT("yi"), TEXT("ייִדיש")},
    {TEXT("yo"), TEXT("Yorùbá")},
    {TEXT("za"), TEXT("Saɯ cueŋ")},
    {TEXT("zh"), TEXT("简体中文")},
    {TEXT("zu"), TEXT("isiZulu")},
    {NULL, NULL}
};

LocaleNativeName* STDCALL GetLocaleNativeName(CTSTR lpCode)
{
    LocaleNativeName *native = &nativeNames[0];
    while(native->lpCode)
    {
        if(scmpi(native->lpCode, lpCode) == 0)
            return native;
        native++;
    }

    return NULL;
}

bool LocaleIsRTL(LocaleStringLookup *l)
{
    return scmpi(l->LookupString(L"RightToLeft"), L"true") == 0;
}

#endif


