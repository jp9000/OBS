/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "Main.h"


//another stripped down code processor from my game engine


#define PeekAtAToken(str) if(!GetNextToken(str, TRUE)) {return FALSE;}
#define HandMeAToken(str) if(!GetNextToken(str)) {return FALSE;}

#define EscapeLikeTheWind(gototoken) {if(!GotoToken(gototoken, TRUE)) {return FALSE;} continue;}

#define ExpectToken(expecting, gototoken) {if(!GetNextToken(curToken)) {return FALSE;} if(curToken != expecting) {if(!GotoToken(gototoken, TRUE)) {return FALSE;} continue;}}
#define ExpectTokenIgnore(expecting) {if(!GetNextToken(curToken)) {return FALSE;} if(curToken != expecting) {continue;}}


CTSTR validSemanticTStrings[] = {TEXT("SV_Position"), TEXT("NORMAL"), TEXT("COLOR"), TEXT("TANGENT"), TEXT("TEXCOORD")};
LPCSTR validSemanticStrings[] = {"SV_Position", "NORMAL", "COLOR", "TANGENT", "TEXCOORD"};

struct SemanticInfo
{
    LPCSTR lpName;
    UINT index;
};

bool GetSemanticInfo(const String &strSemantic, SemanticInfo &info)
{
    for(UINT i=0; i<5; i++)
    {
        UINT semanticLen = slen(validSemanticTStrings[i]);
        if(scmpi_n(strSemantic, validSemanticTStrings[i], semanticLen) == 0)
        {
            if(strSemantic[semanticLen] && isdigit(strSemantic[semanticLen]))
                info.index = strSemantic[semanticLen]-'0';
            else
                info.index = 0;

            info.lpName = validSemanticStrings[i];
            return true;
        }
    }

    return false;
}


BOOL ShaderProcessor::ProcessShader(CTSTR input, CTSTR filename)
{
    String curToken;

    BOOL bError = FALSE;

    SetCodeStart(input);

    TSTR lpLastPos = lpTemp;

    DWORD curInsideCount = 0;
    BOOL  bNewCodeLine = TRUE;

    while(GetNextToken(curToken))
    {
        TSTR lpCurPos = lpTemp-curToken.Length();

        if(curToken[0] == '{')
            ++curInsideCount;
        else if(curToken[0] == '}')
            --curInsideCount;
        else if(curToken[0] == '(')
            ++curInsideCount;
        else if(curToken[0] == ')')
            --curInsideCount;
        else if(curToken[0] == '#') //preprocessor
        {
            HandMeAToken(curToken);
            if(scmpi_n(curToken, TEXT("include"), 7) == 0)
            {
                GetNextToken(curToken);
                if(curToken[0] == '<')
                    EscapeLikeTheWind(TEXT(">")); //TODO: handle #include <foo> directives
                String parent(filename);
                int num = parent.NumTokens('/');
                String loadFile = curToken.Mid(1, curToken.Length()-1);
                parent.FindReplace(parent.GetTokenOffset(num-1, '/'), loadFile);
                
                XFile ShaderFile;

                if(!ShaderFile.Open(parent, XFILE_READ, XFILE_OPENEXISTING))
                    continue;

                String strShader;
                ShaderFile.ReadFileToString(strShader);
                ProcessShader(strShader, parent);
            }
        }
        else if(!curInsideCount && bNewCodeLine) //not inside any code, so this is some sort of declaration (function/struct/var)
        {
            if(curToken == TEXT("class"))
            {
                while(GetNextToken(curToken))
                {
                    if(curToken[0] == '{')
                        curInsideCount++;
                    else if(curToken[0] == '}')
                        curInsideCount--;
                    else if(curToken[0] == ';')
                        if(curInsideCount == 0)
                            continue;
                }
            }
            else if(curToken == TEXT("struct"))
            {
                //try to see if this is the vertex definition structure
                bool bFoundDefinitionStruct = false;

                HandMeAToken(curToken);
                ExpectTokenIgnore(TEXT("{"));
                curInsideCount = 1;

                do 
                {
                    HandMeAToken(curToken);
                    if(curToken.Length() <= 6 && scmpi_n(curToken, TEXT("float"), 5) == 0)
                    {
                        String strType = curToken;

                        String strName;
                        HandMeAToken(strName);

                        HandMeAToken(curToken);
                        if(curToken[0] != ':') //cancel if not a vertex definition structure
                        {
                            bFoundDefinitionStruct = false;
                            break;
                        }

                        String strSemantic;
                        HandMeAToken(strSemantic);

                        SemanticInfo semanticInfo;
                        if(!GetSemanticInfo(strSemantic, semanticInfo))
                        {
                            bFoundDefinitionStruct = false;
                            break;
                        }

                        D3D10_INPUT_ELEMENT_DESC inputElement;
                        inputElement.SemanticName           = semanticInfo.lpName;
                        inputElement.SemanticIndex          = semanticInfo.index;
                        inputElement.InputSlot              = 0;
                        inputElement.AlignedByteOffset      = 0;
                        inputElement.InputSlotClass         = D3D10_INPUT_PER_VERTEX_DATA;
                        inputElement.InstanceDataStepRate   = 0;

                        if(strSemantic.CompareI(TEXT("color")))
                            inputElement.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                        else
                        {
                            switch(strType[5])
                            {
                                case 0:   inputElement.Format = DXGI_FORMAT_R32_FLOAT;          break;
                                case '2': inputElement.Format = DXGI_FORMAT_R32G32_FLOAT;       break;
                                case '3': inputElement.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; break; //todo: check this some time
                                case '4': inputElement.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
                            }
                        }

                        ExpectToken(TEXT(";"), TEXT(";"));

                        PeekAtAToken(curToken);

                        generatedLayout << inputElement;

                        bFoundDefinitionStruct = true;
                    }
                    else
                    {
                        bFoundDefinitionStruct = false;
                        break; //vertex definition structures should really only ever have float values
                    }
                } while (curToken[0] != '}');

                //set up the slots so they match up with vertex buffers
                if(bFoundDefinitionStruct)
                {
                    UINT curSlot = 0;

                    for(UINT i=0; i<generatedLayout.Num(); i++)
                    {
                        if(stricmp(generatedLayout[i].SemanticName, "SV_Position") == 0)
                        {
                            generatedLayout[i].InputSlot = curSlot++;
                            break;
                        }
                    }

                    for(UINT i=0; i<generatedLayout.Num(); i++)
                    {
                        if(stricmp(generatedLayout[i].SemanticName, "NORMAL") == 0)
                        {
                            generatedLayout[i].InputSlot = curSlot++;
                            bHasNormals = true;
                            break;
                        }
                    }

                    for(UINT i=0; i<generatedLayout.Num(); i++)
                    {
                        if(stricmp(generatedLayout[i].SemanticName, "COLOR") == 0)
                        {
                            generatedLayout[i].InputSlot = curSlot++;
                            bHasColors = true;
                            break;
                        }
                    }

                    for(UINT i=0; i<generatedLayout.Num(); i++)
                    {
                        if(stricmp(generatedLayout[i].SemanticName, "TANGENT") == 0)
                        {
                            generatedLayout[i].InputSlot = curSlot++;
                            bHasTangents = true;
                            break;
                        }
                    }

                    bool bFoundTexCoord;

                    do
                    {
                        bFoundTexCoord = false;

                        for(UINT i=0; i<generatedLayout.Num(); i++)
                        {
                            if(generatedLayout[i].SemanticIndex == numTextureCoords && stricmp(generatedLayout[i].SemanticName, "TEXCOORD") == 0)
                            {
                                generatedLayout[i].InputSlot = curSlot++;
                                numTextureCoords++;
                                bFoundTexCoord = true;
                                break;
                            }
                        }
                    } while(bFoundTexCoord);
                }
            }
            else if( (curToken != TEXT("const"))     &&
                     (curToken != TEXT("void"))      &&
                     (curToken != TEXT(";"))         )
            {
                TSTR lpSavedPos = lpTemp;
                String savedToken = curToken;

                if(curToken == TEXT("uniform"))
                    HandMeAToken(curToken);

                String strType = curToken;

                String strName;
                HandMeAToken(strName);

                PeekAtAToken(curToken);
                if(curToken[0] != '(') //verified variable
                {
                    if(strType.CompareI(TEXT("samplerstate")))
                    {
                        ShaderSampler &curSampler = *Samplers.CreateNew();
                        curSampler.name = strName;

                        SamplerInfo info;

                        ExpectToken(TEXT("{"), TEXT(";"));

                        PeekAtAToken(curToken);

                        while(curToken != TEXT("}"))
                        {
                            String strState;
                            HandMeAToken(strState);

                            ExpectToken(TEXT("="), TEXT(";"));

                            String strValue;
                            HandMeAToken(strValue);

                            if(!AddState(info, strState, strValue))
                                EscapeLikeTheWind(TEXT(";"));

                            ExpectToken(TEXT(";"), TEXT(";"));

                            PeekAtAToken(curToken);
                        }

                        curSampler.sampler = CreateSamplerState(info);

                        ExpectToken(TEXT("}"), TEXT("}"));
                        ExpectTokenIgnore(TEXT(";"));

                        //----------------------------------------

                        lpLastPos = lpTemp;
                        continue;
                    }
                    else
                    {
                        ShaderParam *param = Params.CreateNew();
                        param->name = strName;

                        if(curToken[0] == '[')
                        {
                            HandMeAToken(curToken);

                            HandMeAToken(curToken);
                            param->arrayCount = tstoi(curToken);

                            ExpectToken(TEXT("]"), TEXT(";"));

                            PeekAtAToken(curToken);
                        }

                        if(scmpi_n(strType, TEXT("texture"), 7) == 0)
                        {
                            TSTR lpType = strType.Array()+7;
                            supr(lpType);

                            if (!*lpType ||
                                (scmp(lpType, TEXT("1D")) && scmp(lpType, TEXT("2D")) && scmp(lpType, TEXT("3D")) && scmp(lpType, TEXT("CUBE")))
                                )
                            {
                                bError = TRUE;
                            }

                            param->textureID = nTextures++;
                            param->type = Parameter_Texture;

                            strType = TEXT("sampler");
                        }
                        else if(scmp_n(strType, TEXT("float"), 5) == 0)
                        {
                            CTSTR lpType = strType.Array()+5;

                            if(*lpType == 0)
                                param->type = Parameter_Float;
                            else if(scmpi(lpType, TEXT("2")) == 0)
                                param->type = Parameter_Vector2;
                            else if(scmpi(lpType, TEXT("3")) == 0)
                                param->type = Parameter_Vector3;
                            else if(scmpi(lpType, TEXT("4")) == 0)
                                param->type = Parameter_Vector4;
                            else if(scmpi(lpType, TEXT("3x3")) == 0)
                                param->type = Parameter_Matrix3x3;
                            else if(scmpi(lpType, TEXT("4x4")) == 0)
                                param->type = Parameter_Matrix;
                        }
                        else if(scmp(strType, TEXT("int")) == 0)
                            param->type = Parameter_Int;
                        else if(scmp(strType, TEXT("bool")) == 0)
                            param->type = Parameter_Bool;


                        if(curToken == TEXT("="))
                        {
                            HandMeAToken(curToken);

                            BufferOutputSerializer sOut(param->defaultValue);

                            if(scmp(strType, TEXT("float")) == 0)
                            {
                                HandMeAToken(curToken);

                                if(!ValidFloatString(curToken))
                                    bError = TRUE;

                                float fValue = (float)tstof(curToken);

                                sOut << fValue;
                            }
                            else if(scmp(strType, TEXT("int")) == 0)
                            {
                                HandMeAToken(curToken);

                                if(!ValidIntString(curToken))
                                    bError = TRUE;

                                int iValue = tstoi(curToken);

                                sOut << iValue;
                            }
                            else if(scmp_n(strType, TEXT("float"), 5) == 0)
                            {
                                CTSTR lpFloatType = strType.Array()+5;
                                int floatCount = 0;

                                if(lpFloatType[0] == '1') floatCount = 1;
                                else if(lpFloatType[0] == '2') floatCount = 2;
                                else if(lpFloatType[0] == '3') floatCount = 3;
                                else if(lpFloatType[0] == '4') floatCount = 4;
                                else
                                    bError = TRUE;

                                if(lpFloatType[1] == 'x')
                                {
                                    if(lpFloatType[2] != '1')
                                    {
                                        if(lpFloatType[2] == '2') floatCount *= 2;
                                        else if(lpFloatType[2] == '3') floatCount *= 3;
                                        else if(lpFloatType[2] == '4') floatCount *= 4;
                                        else
                                            bError = TRUE;
                                    }
                                }

                                if(floatCount > 1) {ExpectToken(TEXT("{"), TEXT(";"));}

                                int j;
                                for(j=0; j<floatCount; j++)
                                {
                                    if(j)
                                    {
                                        HandMeAToken(curToken);
                                        if(curToken[0] != ',')
                                        {
                                            bError = TRUE;
                                            break;
                                        }
                                    }

                                    HandMeAToken(curToken);

                                    if(!ValidFloatString(curToken))
                                    {
                                        bError = TRUE;
                                        break;
                                    }

                                    float fValue = (float)tstof(curToken);
                                    sOut << fValue;
                                }

                                if(j != floatCount) //processing error occured
                                {
                                    GotoToken(TEXT(";"));
                                    continue;
                                }

                                if(floatCount > 1)
                                {ExpectToken(TEXT("}"), TEXT(";"));}
                            }

                            PeekAtAToken(curToken);
                        }
                    }

                    //--------------------------

                    lpLastPos = lpTemp;
                    bNewCodeLine = FALSE;
                    continue;
                }

                lpTemp = lpSavedPos;
                curToken = savedToken;
            }
        }

        lpLastPos = lpTemp;

        bNewCodeLine = (curToken.IsValid() && ((curToken[0] == ';') || (curToken[0] == '}')));
    }

    return !bError;
}

#undef  ExpectToken
#define ExpectToken(expecting) {if(!GetNextToken(curToken)) {return FALSE;} if(curToken != expecting) {return FALSE;}}

BOOL ShaderProcessor::AddState(SamplerInfo &info, String &stateName, String &stateVal)
{
    if(scmpi_n(stateName, TEXT("Address"), 7) == 0)
    {
        int type = stateName[7]-'U';

        GSAddressMode *mode;
        switch(type)
        {
            case 0: mode = &info.addressU; break;
            case 1: mode = &info.addressV; break;
            case 2: mode = &info.addressW; break;
            default: CrashError(TEXT("Invalid shader address type %d"), type);
        }

        if(stateVal.CompareI(TEXT("Wrap")) || stateVal.CompareI(TEXT("Repeat")))
            *mode = GS_ADDRESS_WRAP;
        else if(stateVal.CompareI(TEXT("Clamp")) || stateVal.CompareI(TEXT("None")))
            *mode = GS_ADDRESS_CLAMP;
        else if(stateVal.CompareI(TEXT("Mirror")))
            *mode = GS_ADDRESS_MIRROR;
        else if(stateVal.CompareI(TEXT("Border")))
            *mode = GS_ADDRESS_BORDER;
        else if(stateVal.CompareI(TEXT("MirrorOnce")))
            *mode = GS_ADDRESS_MIRRORONCE;
    }
    else if(stateName.CompareI(TEXT("MaxAnisotropy")))
    {
        info.maxAnisotropy = tstoi(stateVal);
    }
    else if(stateName.CompareI(TEXT("Filter")))
    {
        if(stateVal.CompareI(TEXT("Anisotropic")))
            info.filter = GS_FILTER_ANISOTROPIC;
        else if(stateVal.CompareI(TEXT("Point")) || stateVal.CompareI(TEXT("MIN_MAG_MIP_POINT")))
            info.filter = GS_FILTER_POINT;
        else if(stateVal.CompareI(TEXT("Linear")) || stateVal.CompareI(TEXT("MIN_MAG_MIP_LINEAR")))
            info.filter = GS_FILTER_LINEAR;
        else if(stateVal.CompareI(TEXT("MIN_MAG_POINT_MIP_LINEAR")))
            info.filter = GS_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        else if(stateVal.CompareI(TEXT("MIN_POINT_MAG_LINEAR_MIP_POINT")))
            info.filter = GS_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        else if(stateVal.CompareI(TEXT("MIN_POINT_MAG_MIP_LINEAR")))
            info.filter = GS_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        else if(stateVal.CompareI(TEXT("MIN_LINEAR_MAG_MIP_POINT")))
            info.filter = GS_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        else if(stateVal.CompareI(TEXT("MIN_LINEAR_MAG_POINT_MIP_LINEAR")))
            info.filter = GS_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        else if(stateVal.CompareI(TEXT("MIN_MAG_LINEAR_MIP_POINT")))
            info.filter = GS_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    }
    else if(stateName.CompareI(TEXT("BorderColor")))
    {
        if(stateVal[0] == '{')
        {
            String curToken;

            HandMeAToken(curToken);
            if(!ValidFloatString(curToken)) {return FALSE;}
            info.borderColor.x = (float)tstof(curToken);

            //-------------------------------

            ExpectToken(TEXT(","));

            //-------------------------------

            HandMeAToken(curToken);
            if(!ValidFloatString(curToken)) {return FALSE;}
            info.borderColor.y = (float)tstof(curToken);

            //-------------------------------

            ExpectToken(TEXT(","));

            //-------------------------------

            HandMeAToken(curToken);
            if(!ValidFloatString(curToken)) {return FALSE;}
            info.borderColor.z = (float)tstof(curToken);

            //-------------------------------

            ExpectToken(TEXT(","));

            //-------------------------------

            HandMeAToken(curToken);
            if(!ValidFloatString(curToken)) {return FALSE;}
            info.borderColor.w = (float)tstof(curToken);

            //-------------------------------

            ExpectToken(TEXT("}"));
        }
        else if(ValidIntString(stateVal))
            info.borderColor = Color4().MakeFromRGBA(tstoi(stateVal));
    }

    return TRUE;
}
