#pragma once

#include <QtCore>
#include <stack>

#include "../REbear.h"
#include <bearparser/bearparser.h>
#include <sig_finder.h>
#include <string>

#include "Releasable.h"
#include "Modification.h"
#include "CommentHandler.h"
#include "ImportsAutoadderSettings.h"
#include "StringsCollection.h"
#include "threads/CollectorThread.h"
#include "threads/SupportedHashes.h"


#define SIZE_UNLIMITED (-1)
//-------------------------------------------------

struct FoundPacker
{
	FoundPacker(offset_t _offset, size_t _size, const std::string& _packerBytes, const std::string& _packerName)
		: offset(_offset), size(_size), packerName(_packerName), packerBytes(_packerBytes)
	{
	}

	FoundPacker(const FoundPacker& p)
	{
		offset = p.offset;
		size = p.size;
		packerName = p.packerName;
		packerBytes = p.packerBytes;
	}
	
	bool operator==(const FoundPacker& rhs) const
	{
		return (offset == rhs.offset && packerBytes == rhs.packerBytes);
	}
	
	offset_t offset;
	size_t size;
	std::string packerName;
	std::string packerBytes;
};

class PeHandler : public QObject, public Releasable
{
	Q_OBJECT

public:
	PeHandler(PEFile *_pe, FileBuffer *_fileBuffer);
	PEFile* getPe() { return m_PE; }

	bool isPeValid() const
	{
		if (!m_PE) return false;

		offset_t lastRva = this->m_PE->getLastMapped(Executable::RVA);
		if (lastRva != m_PE->getImageSize()) {
			return false;
		}
		// TODO: verify the internals of the PE file
		return true;
	}
	
	bool isPeAtypical(QStringList *warnings = NULL)
	{
		bool isAtypical = false;
		if (!isPeValid()) {
			isAtypical = true;
			if (warnings) (*warnings) << tr("The executable may not run: the ImageSize size doesn't fit sections");
		}
		if (m_PE->getImageBase(false) != m_PE->getImageBase(true)) {
			isAtypical = true;
			if (warnings) (*warnings) << tr("The executable has atypical ImageBase. It may be mapped at a default base:") + "0x" + QString::number(m_PE->getImageBase(true), 16);
		}
		if (m_PE->getSectionsCount() == 0) {
			isAtypical = true;
			if (warnings) (*warnings) << tr("The PE has no sections");
		}
		if (isVirtualFormat()) {
			isAtypical = true;
			if (warnings) (*warnings) << tr("The PE is a memory dump in a virtual format (may require unmapping)");
		}
		bool isOk = false;
		const uint64_t machineID = fileHdrWrapper.getNumValue(FileHdrWrapper::MACHINE, &isOk);
		if (isOk && machineID == 0) {
			isAtypical = true;
			if (warnings) (*warnings) << tr("The executable won't run: Machine ID not set");
		}
		const uint64_t subsys = this->optHdrWrapper.getNumValue(OptHdrWrapper::SUBSYS, &isOk);
		if (isOk && subsys == 0) {
			isAtypical = true;
			if (warnings) (*warnings) << tr("The executable won't run: Subsystem not set");
		}
		const uint64_t optHdrMagic = this->optHdrWrapper.getNumValue(OptHdrWrapper::MAGIC, &isOk);
		if (isOk && optHdrMagic == 0) {
			isAtypical = true;
			if (warnings) (*warnings) << tr("The executable won't run: OptHdr Magic not set");
		}
		const size_t mappedSecCount = m_PE->getSectionsCount(true);
		// check for unaligned sections:
		if (mappedSecCount != m_PE->getSectionsCount(false)) {
			isAtypical = true;
			if (warnings) (*warnings) << tr("Not all sections are mapped");
		}
		for (size_t i = 0; i < mappedSecCount; i++) {
			SectionHdrWrapper *sec = m_PE->getSecHdr(i);
			const offset_t hdrOffset = sec->getContentOffset(Executable::RAW, false);
			const offset_t mappedOffset = sec->getContentOffset(Executable::RAW, true);
			if (mappedOffset == INVALID_ADDR) {
				isAtypical = true;
				if (warnings) (*warnings) << tr("The PE may be truncated. Some sections are outside the file scope.");
				break;
			}
			else if (hdrOffset != mappedOffset) {
				isAtypical = true;
				if (warnings) (*warnings) << tr("Contains sections misaligned to FileAlignment");
				break;
			}
		}
		// Warn about possible Mixed Mode PEs
		if (hasDirectory(pe::DIR_COM_DESCRIPTOR)) {
			uint64_t flags = this->clrDirWrapper.getNumValue(ClrDirWrapper::FLAGS, &isOk);
			if (isOk && (flags & pe::COMIMAGE_FLAGS_ILONLY) == 0) {
				isAtypical = true;
				if (warnings) (*warnings) << tr("This .NET file may contain native code.");
			}
		}
		return isAtypical;
	}

	bool updateFileModifTime()
	{
		QDateTime modDate = QDateTime(); //default: empty date
		const QString path = this->getFullName();
		QFileInfo fileInfo(path);
		if (fileInfo.exists()) {
			QFileInfo fileInfo = QFileInfo(path);
			modDate = fileInfo.lastModified();
		}
		const QDateTime prevDate = this->m_fileModDate;
		if (prevDate.toMSecsSinceEpoch() == modDate.toMSecsSinceEpoch()) {
			// no need to update:
			return false;
		}
		this->m_fileModDate = modDate;
		return true;
	}
	
	bool isFileOnDiskChanged()
	{
		if (this->m_fileModDate.toMSecsSinceEpoch() == this->m_loadedFileModDate.toMSecsSinceEpoch()) {
			// the loaded file is the same as the file on the disk
			return false;
		}
		return true;
	}

	bool hasDirectory(pe::dir_entry dirNum) const;

	QString getCurrentSHA256() { return getCurrentHash(SupportedHashes::SHA256); }
	QString getCurrentMd5() { return getCurrentHash(SupportedHashes::MD5); }
	QString getCurrentSHA1() { return getCurrentHash(SupportedHashes::SHA1); }
	QString getCurrentChecksum() { return getCurrentHash(SupportedHashes::CHECKSUM); }
	QString getRichHdrHash() { return getCurrentHash(SupportedHashes::RICH_HDR_MD5); }
	QString getImpHash() { return getCurrentHash(SupportedHashes::IMP_MD5); }
	QString getCurrentHash(SupportedHashes::hash_type type);

	void setPackerSignFinder(sig_finder::Node* signFinder);
	bool isPacked();
	size_t findPackerSign(offset_t startOff, Executable::addr_type addrType);

	/* fetch info about offset */
	bool isInActiveArea(offset_t offset);
	bool isInModifiedArea(offset_t offset);

	/* resize */
	bool resize(bufsize_t newSize, bool continueLastOperation = false);
	bool resizeImage(bufsize_t newSize);
	
	bool setByte(offset_t offset, BYTE val);

	bool isVirtualFormat();
	bool isVirtualEqualRaw();
	bool copyVirtualSizesToRaw();

	SectionHdrWrapper* addSection(QString name,  bufsize_t rSize, bufsize_t vSize); //throws exception
	offset_t loadSectionContent(SectionHdrWrapper* sec, QFile &fIn, bool continueLastOperation = false);

	bool moveDataDirEntry(pe::dir_entry dirNum, offset_t targetRaw);

	size_t getDirSize(pe::dir_entry dirNum);
	bool canAddImportsLib(size_t libsCount);
	bool addImportLib(bool continueLastOperation = false);
	bool addImportFunc(size_t parentLibNum);
	
	bool autoAddImports(const ImportsAutoadderSettings &settings); //throws CustomException

	void setEP(offset_t newEpRva);

	/* content manipulation / substitution */
	bool clearBlock(offset_t offset, uint64_t size);
	bool fillBlock(offset_t offset, uint64_t size, BYTE val);
	bool substBlock(offset_t offset, uint64_t size, BYTE* buf);

	/* modifications */
	bool isDataDirModified(offset_t modOffset, bufsize_t modSize);
	bool isSectionsHeadersModified(offset_t modOffset, bufsize_t modSize);
	void backupModification(offset_t  modOffset, bufsize_t modSize, bool continueLastOp = false);
	void backupResize(bufsize_t newSize, bool continueLastOperation = false);
	void unbackupLastModification();
	bool undoLastModification();
	bool setBlockModified(offset_t  modOffset, bufsize_t modSize);

	void unModify();
	bool isPEModified() { return this->modifHndl.countOperations() ? true : false;  }

	/* display */
	bool markedBranching(offset_t origin, offset_t target);
	bool setDisplayed(bool isRVA, offset_t displayedOffset, bufsize_t displayedSize = SIZE_UNLIMITED);

	offset_t getDisplayedOffset() { return displayedOffset; }
	size_t getDisplayedSize() { return displayedSize; }

	void setHilighted(offset_t hilightedOffset, bufsize_t hilightedSize);
	void setHovered(bool isRVA, offset_t hilightedOffset, bufsize_t hilightedSize);

	void setPageOffset(offset_t pageOffset);
	void advanceOffset(int increment);

	bool setDisplayedEP();
	void undoDisplayOffset();

	bool exportDisasm(const QString &path, const offset_t startOff, const size_t previewSize);

	/* File name wrappers */
	QString getFullName() { return this->m_fileBuffer->getFileName(); }

	QString getShortName()
	{
		const QString path = getFullName();
		QFileInfo fileInfo(path);
		return fileInfo.fileName();
	}

	QString getDirPath()
	{
		const QString path = getFullName();
		QFileInfo fileInfo(path);
		return fileInfo.absoluteDir().absolutePath();
	}
	
//--------
	/* wrappers for PE structures */
	DosHdrWrapper dosHdrWrapper;
	RichHdrWrapper richHdrWrapper;
	FileHdrWrapper fileHdrWrapper;
	OptHdrWrapper optHdrWrapper;
	DataDirWrapper dataDirWrapper;

	ResourcesAlbum &resourcesAlbum;

	/*Directory wrappers */
	ExportDirWrapper &exportDirWrapper;
	ImportDirWrapper &importDirWrapper;
	TlsDirWrapper &tlsDirWrapper;
	RelocDirWrapper &relocDirWrapper;
	SecurityDirWrapper &securityDirWrapper;
	LdConfigDirWrapper &ldConfDirWrapper;
	BoundImpDirWrapper &boundImpDirWrapper;
	DelayImpDirWrapper &delayImpDirWrapper;
	DebugDirWrapper &debugDirWrapper;
	ClrDirWrapper &clrDirWrapper;
	ExceptionDirWrapper &exceptDirWrapper;
	ResourceDirWrapper &resourcesDirWrapper;

	ExeElementWrapper* dataDirWrappers[pe::DIR_ENTRIES_COUNT]; // Pointers to above wrappers

	/* editon related handlers */
	CommentHandler comments;
	ModificationHandler modifHndl;
	//---
	offset_t markedOrigin, markedTarget;//, markedOriginRaw, markedTargetRaw;
	offset_t displayedOffset, displayedSize;
	offset_t hilightedOffset, hilightedSize;
	offset_t hoveredOffset, hoveredSize;

	offset_t pageStart;
	bufsize_t pageSize;
	std::stack<offset_t> prevOffsets;
	std::vector<FoundPacker> packerAtOffset;
	StringsCollection stringsMap;
	
signals:
	void pageOffsetModified(offset_t pageStart, bufsize_t pageSize);

	void modified();
	void secHeadersModified();
	void marked();
	void hovered();

	void foundSignatures(int count, int requestType);
	void hashChanged();
	void stringsUpdated();
	void stringsLoadingProgress(int progress);

protected slots:
	// hashes:
	void onHashReady(QString hash, int hType);
	void runHashesCalculation();
	
	// strings extraction:
	bool runStringsExtraction();
	void onStringsReady(StringsCollection *mapToFill);

	void onStringsLoadingProgress(int progress)
	{
		emit stringsLoadingProgress(progress); // forward the signal
	}

protected:
	ImportEntryWrapper* _autoAddLibrary(const QString &name, size_t importedFuncsCount, size_t expectedDllsCount, offset_t &storageOffset, bool separateOFT, bool continueLastOperation = false); //throws CustomException
	bool _autoFillFunction(ImportEntryWrapper* libWr, ImportedFuncWrapper* func, const QString& name, const WORD ordinal, offset_t &storageOffset); //throws CustomException
	
	ImportedFuncWrapper* _addImportFunc(ImportEntryWrapper *lib, bool continueLastOperation = false);
	bool _moveDataDirEntry(pe::dir_entry dirNum, offset_t targetRaw, bool continueLastOperation = false);
	
	size_t _getThunkSize() const
	{
		return m_PE->isBit64() ? sizeof(uint64_t) : sizeof(uint32_t);
	}
	
	~PeHandler() {
		deleteThreads();
		if (m_PE) {
			delete m_PE;
			m_PE = NULL;
		}
		if (m_fileBuffer) {
			delete m_fileBuffer;
			m_fileBuffer = NULL;
		}
	}
	
	void deleteThreads();

	void associateWrappers();

	bool isBaseHdrModif(offset_t modifOffset, bufsize_t size);
	
	bool rewrapDataDirs();

	bool updatePeOnModified(offset_t modOffset = INVALID_ADDR, bufsize_t modSize = 0);// throws exception
	void updatePeOnResized();

	PEFile* m_PE;
	FileBuffer *m_fileBuffer;
	QMutex m_UpdateMutex;

	QDateTime m_fileModDate; //modification time of the corresponding file on the disk
	QDateTime m_loadedFileModDate; //modification time of the version that is currently loaded

	CollectorThreadManager *hashCalcMgrs[SupportedHashes::HASHES_NUM];

	QString hash[SupportedHashes::HASHES_NUM];
	QMutex m_hashMutex[SupportedHashes::HASHES_NUM];
	
	CollectorThreadManager* stringThreadMgr;

	sig_finder::Node *signFinder;
};
