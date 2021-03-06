//
// Copyright (c) 2007 Ole Andr� Vadla Ravn�s <oleavr@gmail.com>
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#pragma once

#include "Logging.h"

namespace InterceptPP {

#pragma warning (push)
#pragma warning (disable: 4251)

class INTERCEPTPP_API IPropertyProvider
{
public:
    virtual bool QueryForProperty(const OString &query, int &result) = 0;
    virtual bool QueryForProperty(const OString &query, unsigned int &result) = 0;
    virtual bool QueryForProperty(const OString &query, void *&result) = 0;
    virtual bool QueryForProperty(const OString &query, va_list &result) = 0;
    virtual bool QueryForProperty(const OString &query, OString &result) = 0;
};

class INTERCEPTPP_API PropertyOverrides : public BaseObject
{
public:
    void Add(const OString &propName, const OString &value);
    bool Contains(const OString &propName) const;
    bool GetValue(const OString &propName, OString &value) const;
    bool GetValue(const OString &propName, int &value) const;
    bool GetValue(const OString &propName, unsigned int &value) const;

protected:
    typedef OMap<OString, OString>::Type OverridesMap;
    OverridesMap m_overrides;
};

class INTERCEPTPP_API BaseMarshaller : public BaseObject
{
public:
    BaseMarshaller(const OString &typeName);
    virtual ~BaseMarshaller() {};

    virtual BaseMarshaller *Clone() const { return NULL; }

    virtual bool SetProperty(const OString &name, const OString &value) { return false; }

    bool HasPropertyBinding(const OString &propName) const;
    const OString &GetPropertyBinding(const OString &propName) const;
    void SetPropertyBinding(const OString &propName, const OString &value) { m_propBindings[propName] = value; }
    void SetPropertyBindings(const char *firstPropName, ...);

    virtual const OString &GetName() const { return m_typeName; }
    virtual unsigned int GetSize() const = 0;
    virtual Logging::Node *ToNode(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const = 0;
    virtual bool ToInt(void *start, int &result) const { return false; }
    virtual bool ToUInt(void *start, unsigned int &result) const { return false; }
    virtual bool ToPointer(void *start, void *&result) const { return false; }
    virtual bool ToVaList(void *start, va_list &result) const { return false; }

protected:
    OString m_typeName;
    typedef OMap<OString, OString>::Type PropBindingsMap;
    PropBindingsMap m_propBindings;
};

namespace Marshaller {

typedef BaseMarshaller *(*CreateMarshallerFunc) (const OString &name);

class INTERCEPTPP_API Factory : public BaseObject
{
public:
    static Factory *Instance();
    Factory();

    BaseMarshaller *CreateMarshaller(const OString &name);

    bool HasMarshaller(const OString &name);
    void RegisterMarshaller(const OString &name, CreateMarshallerFunc createFunc);
    void UnregisterMarshaller(const OString &name);

protected:
    typedef OMap<OString, CreateMarshallerFunc>::Type MarshallerMap;
    MarshallerMap m_marshallers;

    template<class T> static BaseMarshaller *CreateMarshallerInstance(const OString &name);
};

class INTERCEPTPP_API Dynamic : public BaseMarshaller
{
public:
    Dynamic();
    Dynamic(const Dynamic &instance);
    virtual ~Dynamic();

    virtual BaseMarshaller *Clone() const { return new Dynamic(*this); }

    virtual bool SetProperty(const OString &name, const OString &value);

    virtual unsigned int GetSize() const { return m_fallback->GetSize(); }
    virtual Logging::Node *ToNode(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;

protected:
    OString m_nameBase;
    OString m_nameSuffix;
    BaseMarshaller *m_fallback;

    OString ToStringInternal(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides, OString *lastSubTypeName) const;
    BaseMarshaller *CreateMarshaller(IPropertyProvider *propProv) const;
};

class INTERCEPTPP_API Pointer : public BaseMarshaller
{
public:
    Pointer(BaseMarshaller *type=NULL, const OString &ptrTypeName="Pointer");
    Pointer(const Pointer &p);
    virtual ~Pointer();

    virtual BaseMarshaller *Clone() const { return new Pointer(*this); }

    virtual bool SetProperty(const OString &name, const OString &value);

    virtual unsigned int GetSize() const { return sizeof(void *); }
    virtual Logging::Node *ToNode(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual bool ToInt(void *start, int &result) const;
    virtual bool ToUInt(void *start, unsigned int &result) const;
    virtual bool ToPointer(void *start, void *&result) const;

protected:
    BaseMarshaller *m_type;
};

class INTERCEPTPP_API VaList : public BaseMarshaller
{
public:
    VaList()
        : BaseMarshaller("VaList")
    {}

    virtual BaseMarshaller *Clone() const { return new VaList(*this); }

    virtual unsigned int GetSize() const { return sizeof(va_list); }
    virtual Logging::Node *ToNode(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual bool ToVaList(void *start, va_list &result) const;
};

template <class T>
class INTERCEPTPP_API Integer : public BaseMarshaller
{
public:
    Integer(const OString &typeName, bool sign=true, bool hex=false)
        : BaseMarshaller(typeName),
          m_bigEndian(false),
          m_sign(sign),
          m_hex(hex)
    {}

    virtual bool SetProperty(const OString &name, const OString &value);

    virtual unsigned int GetSize() const { return sizeof(T); }
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual bool ToInt(void *start, int &result) const;
    virtual bool ToUInt(void *start, unsigned int &result) const;

    bool GetFormatHex() const { return m_hex; }
    void SetFormatHex(bool hex) { m_hex = hex; }

protected:
    bool m_bigEndian;
    bool m_sign;
    bool m_hex;

    virtual T ToLittleEndian(T i) const { return i; }
};

class INTERCEPTPP_API UInt8 : public Integer<unsigned char>
{
public:
    UInt8(bool hex=false)
        : Integer("UInt8", false, hex)
    {}

    virtual BaseMarshaller *Clone() const { return new UInt8(*this); }
};

class INTERCEPTPP_API UInt8Ptr : public Pointer
{
public:
    UInt8Ptr(bool hex=false)
        : Pointer(new UInt8(hex))
    {}
};

class INTERCEPTPP_API Int8 : public Integer<short>
{
public:
    Int8(bool hex=false)
        : Integer("Int8", true, hex)
    {}

    virtual BaseMarshaller *Clone() const { return new Int8(*this); }
};

class INTERCEPTPP_API Int8Ptr : public Pointer
{
public:
    Int8Ptr(bool hex=false)
        : Pointer(new Int8(hex))
    {}
};

class INTERCEPTPP_API UInt16 : public Integer<unsigned short>
{
public:
    UInt16(bool hex=false)
        : Integer("UInt16", false, hex)
    {}

    virtual BaseMarshaller *Clone() const { return new UInt16(*this); }

protected:
    virtual unsigned short ToLittleEndian(unsigned short i) const { return _byteswap_ushort(i); }
};

class INTERCEPTPP_API UInt16Ptr : public Pointer
{
public:
    UInt16Ptr(bool hex=false)
        : Pointer(new UInt16(hex))
    {}
};

class INTERCEPTPP_API Int16 : public Integer<short>
{
public:
    Int16(bool hex=false)
        : Integer("Int16", true, hex)
    {}

    virtual BaseMarshaller *Clone() const { return new Int16(*this); }

protected:
    virtual short ToLittleEndian(short i) const { return _byteswap_ushort(i); }
};

class INTERCEPTPP_API Int16Ptr : public Pointer
{
public:
    Int16Ptr(bool hex=false)
        : Pointer(new Int16(hex))
    {}
};

class INTERCEPTPP_API UInt32 : public Integer<unsigned int>
{
public:
    UInt32(bool hex=false)
        : Integer("UInt32", false, hex)
    {}

    virtual BaseMarshaller *Clone() const { return new UInt32(*this); }

protected:
    virtual unsigned int ToLittleEndian(unsigned int i) const { return _byteswap_ulong(i); }
};

class INTERCEPTPP_API UInt32Ptr : public Pointer
{
public:
    UInt32Ptr(bool hex=false)
        : Pointer(new UInt32(hex))
    {}
};

class INTERCEPTPP_API Int32 : public Integer<int>
{
public:
    Int32(bool hex=false)
        : Integer("Int32", true, hex)
    {}

    virtual BaseMarshaller *Clone() const { return new Int32(*this); }

protected:
    virtual int ToLittleEndian(int i) const { return _byteswap_ulong(i); }
};

class INTERCEPTPP_API Int32Ptr : public Pointer
{
public:
    Int32Ptr(bool hex=false)
        : Pointer(new Int32(hex))
    {}
};

class INTERCEPTPP_API Boolean : public BaseMarshaller
{
public:
    Boolean();
    Boolean(BaseMarshaller *marshaller);
    Boolean(const Boolean &b);
    virtual ~Boolean();

    virtual BaseMarshaller *Clone() const { return new Boolean(*this); }

    virtual bool SetProperty(const OString &name, const OString &value);

    virtual unsigned int GetSize() const { return m_marshaller->GetSize(); }
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual bool ToInt(void *start, int &result) const { return m_marshaller->ToInt(start, result); }
    virtual bool ToUInt(void *start, unsigned int &result) const { return m_marshaller->ToUInt(start, result); }

protected:
    BaseMarshaller *m_marshaller;
    OString m_trueStr;
    OString m_falseStr;
};

class INTERCEPTPP_API CPPBool : public Boolean
{
public:
    CPPBool()
        : Boolean(new UInt8())
    {}

    virtual BaseMarshaller *Clone() const { return new CPPBool(*this); }
};

class INTERCEPTPP_API MSBool : public Boolean
{
public:
    MSBool()
        : Boolean(new UInt32())
    {
        m_trueStr = "TRUE";
        m_falseStr = "FALSE";
    }

    virtual BaseMarshaller *Clone() const { return new MSBool(*this); }
};

class INTERCEPTPP_API Array : public BaseMarshaller
{
public:
    Array();
    Array(BaseMarshaller *elType, unsigned int elCount);
    Array(BaseMarshaller *elType, const OString &elCountPropertyBinding);
    Array(const Array &a);
    virtual ~Array();

    virtual BaseMarshaller *Clone() const { return new Array(*this); }

    virtual bool SetProperty(const OString &name, const OString &value);

    virtual unsigned int GetSize() const;
    virtual Logging::Node *ToNode(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;

protected:
    BaseMarshaller *m_elType;
    unsigned int m_elCount;
};

class INTERCEPTPP_API ArrayPtr : public Pointer
{
public:
    ArrayPtr()
        : Pointer(new Array())
    {}

    ArrayPtr(BaseMarshaller *elType, unsigned int elCount)
        : Pointer(new Array(elType, elCount))
    {}

    ArrayPtr(BaseMarshaller *elType, const OString &elCountPropertyBinding)
        : Pointer(new Array(elType, elCountPropertyBinding))
    {}
};

class INTERCEPTPP_API ByteArray : public BaseMarshaller
{
public:
    ByteArray(int size=0);
    ByteArray(const OString &sizePropertyBinding);

    virtual bool SetProperty(const OString &name, const OString &value);

    virtual unsigned int GetSize() const { return m_size; }
    virtual Logging::Node *ToNode(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;

protected:
    int m_size;
};

class INTERCEPTPP_API ByteArrayPtr : public Pointer
{
public:
    ByteArrayPtr(int size=0)
        : Pointer(new ByteArray(size))
    {}

    ByteArrayPtr(const OString &sizePropertyBinding)
        : Pointer(new ByteArray(sizePropertyBinding))
    {}
};

class INTERCEPTPP_API CString : public BaseMarshaller
{
public:
    CString(const OString &typeName, unsigned int elementSize, int length)
        : BaseMarshaller(typeName),
          m_elementSize(elementSize),
          m_length(length)
    {}

    virtual unsigned int GetSize() const { return m_elementSize * (m_length + 1); }

protected:
    unsigned int m_elementSize;
    int m_length;
};

class INTERCEPTPP_API AsciiString : public CString
{
public:
    AsciiString(int length=-1)
        : CString("AsciiString", sizeof(CHAR), length)
    {}

    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
};

class INTERCEPTPP_API AsciiStringPtr : public Pointer
{
public:
    AsciiStringPtr()
        : Pointer(new AsciiString())
    {}
};

class INTERCEPTPP_API AsciiFormatString : public CString
{
public:
    AsciiFormatString(int length=-1)
        : CString("AsciiFormatString", sizeof(CHAR), length)
    {}

    virtual bool SetProperty(const OString &name, const OString &value);

    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
};

class INTERCEPTPP_API AsciiFormatStringPtr : public Pointer
{
public:
    AsciiFormatStringPtr()
        : Pointer(new AsciiFormatString())
    {}
};

class INTERCEPTPP_API UnicodeString : public CString
{
public:
    UnicodeString(int length=-1)
        : CString("UnicodeString", sizeof(WCHAR), length)
    {}

    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
};

class INTERCEPTPP_API UnicodeStringPtr : public Pointer
{
public:
    UnicodeStringPtr()
        : Pointer(new UnicodeString())
    {}
};

class INTERCEPTPP_API UnicodeFormatString : public CString
{
public:
    UnicodeFormatString(int length=-1)
        : CString("UnicodeFormatString", sizeof(WCHAR), length)
    {}

    virtual bool SetProperty(const OString &name, const OString &value);

    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
};

class INTERCEPTPP_API UnicodeFormatStringPtr : public Pointer
{
public:
    UnicodeFormatStringPtr()
        : Pointer(new UnicodeFormatString())
    {}
};

class INTERCEPTPP_API Enumeration : public BaseMarshaller
{
public:
    Enumeration(const char *name, BaseMarshaller *marshaller, const char *firstName, ...);
    Enumeration(const Enumeration &e);

    virtual BaseMarshaller *Clone() const { return new Enumeration(*this); }

    virtual bool SetProperty(const OString &name, const OString &value);

    bool AddMember(const OString &name, DWORD value);
    unsigned int GetMemberCount() const { return static_cast<unsigned int>(m_defs.size()); }

    virtual unsigned int GetSize() const { return m_marshaller->GetSize(); }
    virtual Logging::Node *ToNode(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual bool ToInt(void *start, int &result) const { return m_marshaller->ToInt(start, result); }
    virtual bool ToUInt(void *start, unsigned int &result) const { return m_marshaller->ToUInt(start, result); }

protected:
    OMap<DWORD, OString>::Type m_defs;

    BaseMarshaller *m_marshaller;
};

class INTERCEPTPP_API StructureField : public BaseObject
{
public:
    StructureField(const OString &name, DWORD offset, BaseMarshaller *marshaller)
        : m_name(name), m_offset(offset), m_marshaller(marshaller)
    {}

    const OString &GetName() const { return m_name; }
    DWORD GetOffset() const { return m_offset; }
    const BaseMarshaller *GetMarshaller() const { return m_marshaller; }

protected:
    OString m_name;
    DWORD m_offset;
    BaseMarshaller *m_marshaller;
};

class INTERCEPTPP_API FieldTypePropertyBinding : public BaseObject
{
public:
    FieldTypePropertyBinding(const OString &fieldName, const OString &propName, const OString &srcFieldName)
        : m_fieldName(fieldName), m_propName(propName), m_srcFieldName(srcFieldName)
    {}

    const OString &GetFieldName() const { return m_fieldName; }
    const OString &GetPropertyName() const { return m_propName; }
    const OString &GetSourceFieldName() const { return m_srcFieldName; }

protected:
    OString m_fieldName;
    OString m_propName;
    OString m_srcFieldName;
};

class INTERCEPTPP_API Structure : public BaseMarshaller
{
public:
    Structure(const char *name, const char *firstFieldName, ...);
    Structure(const char *name, const char *firstFieldName, va_list args);
    Structure(const Structure &s);
    ~Structure();

    virtual BaseMarshaller *Clone() const { return new Structure(*this); }

    void AddField(StructureField *field);
    unsigned int GetFieldCount() const { return static_cast<unsigned int>(m_fields.size()); }
    void BindFieldTypePropertyToField(const OString &fieldName, const OString &propName, const OString &srcFieldName);

    virtual unsigned int GetSize() const { return m_size; }
    virtual Logging::Node *ToNode(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;

protected:
    typedef OVector<StructureField *>::Type FieldsVector;
    typedef OMap<OString, unsigned int>::Type FieldIndexesMap;
    typedef OVector<FieldTypePropertyBinding>::Type FieldBindingsVector;

    unsigned int m_size;
    FieldsVector m_fields;
    FieldIndexesMap m_fieldIndexes;
    FieldBindingsVector m_bindings;
 
    void Initialize(const char *firstFieldName, va_list args);
    PropertyOverrides **GetFieldOverrides(void *start, IPropertyProvider *propProv) const;
    void FreeFieldOverrides(PropertyOverrides **fieldOverrides) const;
};

class INTERCEPTPP_API StructurePtr : public Pointer
{
public:
    StructurePtr(const char *firstFieldName, ...);
};

class INTERCEPTPP_API Ipv4InAddr : public BaseMarshaller
{
public:
    Ipv4InAddr()
        : BaseMarshaller("Ipv4InAddr")
    {}

    virtual unsigned int GetSize() const { return sizeof(DWORD); }
    virtual OString ToString(void *start, bool deep, IPropertyProvider *propProv, PropertyOverrides *overrides=NULL) const;
};

} // namespace Marshaller

#pragma warning (pop)

} // namespace InterceptPP
