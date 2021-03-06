/*
TER-Server
*/


#ifndef DB2STORE_H
#define DB2STORE_H

#include "DB2FileLoader.h"

#include "ByteBuffer.h"
#include <vector>

/// Interface class for common access
 class DB2StorageBase
 {
	public:
		virtual ~DB2StorageBase() { }
		
			uint32 GetHash() const { return tableHash; }
		
			virtual bool HasRecord(uint32 id) const = 0;
		
			virtual void WriteRecord(uint32 id, ByteBuffer& buffer) const = 0;
		
			protected:
				uint32 tableHash;
				};

template<class T>
class DB2Storage;

template<class T>
bool DB2StorageHasEntry(DB2Storage<T> const& store, uint32 id)
 {
	return store.LookupEntry(id) != NULL;
	}

template<class T>
void WriteDB2RecordToPacket(DB2Storage<T> const& store, uint32 id, ByteBuffer& buffer)
 {
	uint8 const* entry = (uint8 const*)store.LookupEntry(id);
	ASSERT(entry);
	
		std::string format = store.GetFormat();
	for (uint32 i = 0; i < format.length(); ++i)
		 {
		switch (format[i])
			 {
			case FT_IND:
				case FT_INT:
					buffer << *(uint32*)entry;
					entry += 4;
					break;
					case FT_FLOAT:
						buffer << *(float*)entry;
						entry += 4;
						break;
						case FT_BYTE:
							buffer << *(uint8*)entry;
							entry += 1;
							break;
							case FT_STRING:
								{
									size_t len = strlen(*(char**)entry);
									buffer << uint16(len);
									if (len)
									buffer << *(char**)entry;
									entry += sizeof(char*);
									break;
									}
							case FT_NA:
									case FT_SORT:
										buffer << uint32(0);
										break;
										case FT_NA_BYTE:
											buffer << uint8(0);
											break;
											}
		}
	}

template<class T>
class DB2Storage : public DB2StorageBase
{
    typedef std::list<char*> StringPoolList;
    typedef std::vector<T*> DataTableEx;
	typedef bool(*EntryChecker)(DB2Storage<T> const&, uint32);
	typedef void(*PacketWriter)(DB2Storage<T> const&, uint32, ByteBuffer&);
public:
	DB2Storage(char const* f, EntryChecker checkEntry = NULL, PacketWriter writePacket = NULL) :
		nCount(0), fieldCount(0), fmt(f), indexTable(NULL), m_dataTable(NULL)
		 {
		CheckEntry = checkEntry ? checkEntry : &DB2StorageHasEntry<T>;
		WritePacket = writePacket ? writePacket : &WriteDB2RecordToPacket<T>;
		}
    ~DB2Storage() { Clear(); }

	bool HasRecord(uint32 id) const { return CheckEntry(*this, id); }
    T const* LookupEntry(uint32 id) const { return (id >= nCount) ? NULL : indexTable[id]; }
    uint32 GetNumRows() const { return nCount; }
    char const* GetFormat() const { return fmt; }
    uint32 GetFieldCount() const { return fieldCount; }
	void WriteRecord(uint32 id, ByteBuffer& buffer) const
		 {
		WritePacket(*this, id, buffer);
		}
    
    T* CreateEntry(uint32 id, bool evenIfExists = false)
    {
        if (evenIfExists && LookupEntry(id))
            return NULL;

        if (id >= nCount)
        {
            // reallocate index table
            char** tmpIdxTable = new char*[id + 1];
            memset(tmpIdxTable, 0, (id + 1) * sizeof(char*));
            memcpy(tmpIdxTable, (char*)indexTable, nCount * sizeof(char*));
            delete[] ((char*)indexTable);
            nCount = id + 1;
            indexTable = (T**)tmpIdxTable;
        }

        T* entryDst = new T;
        m_dataTableEx.push_back(entryDst);
        indexTable[id] = entryDst;
        return entryDst;
    }

    void EraseEntry(uint32 id) { indexTable[id] = NULL; }

    bool Load(char const* fn)
    {
        DB2FileLoader db2;
        // Check if load was sucessful, only then continue
        if (!db2.Load(fn, fmt))
            return false;

        fieldCount = db2.GetCols();
		tableHash = db2.GetHash();

        // load raw non-string data
        m_dataTable = (T*)db2.AutoProduceData(fmt, nCount, (char**&)indexTable);

        // create string holders for loaded string fields
        m_stringPoolList.push_back(db2.AutoProduceStringsArrayHolders(fmt, (char*)m_dataTable));

        // load strings from dbc data
        m_stringPoolList.push_back(db2.AutoProduceStrings(fmt, (char*)m_dataTable));

        // error in dbc file at loading if NULL
        return indexTable!=NULL;
    }

    bool LoadStringsFrom(char const* fn)
    {
        // DBC must be already loaded using Load
        if (!indexTable)
            return false;

        DB2FileLoader db2;
        // Check if load was successful, only then continue
        if (!db2.Load(fn, fmt))
            return false;

        // load strings from another locale dbc data
        m_stringPoolList.push_back(db2.AutoProduceStrings(fmt, (char*)m_dataTable));

        return true;
    }

    void Clear()
    {
        if (!indexTable)
            return;

        delete[] ((char*)indexTable);
        indexTable = NULL;

        delete[] ((char*)m_dataTable);
        m_dataTable = NULL;

        for (typename DataTableEx::iterator itr = m_dataTableEx.begin(); itr != m_dataTableEx.end(); ++itr)
            delete *itr;
        m_dataTableEx.clear();

        while (!m_stringPoolList.empty())
        {
            delete[] m_stringPoolList.front();
            m_stringPoolList.pop_front();
        }

        nCount = 0;
    }

	EntryChecker CheckEntry;
	PacketWriter WritePacket;
	
private:
    uint32 nCount;
    uint32 fieldCount;
    char const* fmt;
    T** indexTable;
    T* m_dataTable;
    DataTableEx m_dataTableEx;
    StringPoolList m_stringPoolList;
};

#endif