#include "CalcThread.h"

//--- 
inline QString stripExtension(const QString & fileName)
{
    return fileName.left(fileName.lastIndexOf("."));
}

QString CalcThread::makeImpHash()
{
	static CommonOrdinalsLookup lookup;
	ImportDirWrapper* imports = m_PE->getImportsDir();
	if (!imports) return QString();

	QStringList exts;
	exts.append(".ocx");
	exts.append(".sys");
	exts.append(".dll");

	const size_t librariesCount = imports->getEntriesCount();

	QList<offset_t> thunks = imports->getThunksList();
	const size_t functionsCount = thunks.size();

	QStringList impsBlock;
	for (int i = 0; i < thunks.size(); i++) {
		offset_t thunk = thunks[i];
		if (thunk == 0 || thunk == INVALID_ADDR) continue;

		QString lib =  imports->thunkToLibName(thunk).toLower();
		for (QStringList::iterator itr = exts.begin(); itr != exts.end(); ++itr) {
			if (lib.endsWith(*itr)) {
				lib = stripExtension(lib);
				break;
			}
		}
		ImportBaseFuncWrapper* func = imports->thunkToFunction(thunk);
		if (!func) continue;
		QString funcName;
		if (!func->isByOrdinal()) {
			funcName = func->getShortName();
		} else {
			int ord = func->getOrdinal();
			funcName = lookup.findFuncName(lib, ord);
			if (!funcName.length()) {
				funcName = "ord" + QString::number(ord, 10);
			}
		}
		impsBlock << QString(lib + "." + funcName.toLower());
	}

	const QString allImps = impsBlock.join(",");
	//std::cout << allImps.toStdString() << "\n";
	QCryptographicHash calcHash(QCryptographicHash::Md5);
	calcHash.addData((char*) allImps.toStdString().c_str(), allImps.length());
	return QString(calcHash.result().toHex());
}

QString CalcThread::makeRichHdrHash()
{
	pe::RICH_SIGNATURE* sign = m_PE->getRichHeaderSign();
	pe::RICH_DANS_HEADER* dans = NULL;
	if (sign) {
		dans = m_PE->getRichHeaderBgn(sign);
	}
	if (!dans) {
		return QString();
	}
	const size_t diff = (ULONGLONG)sign - (ULONGLONG)dans;
	ByteBuffer tmpBuf((BYTE*)dans, diff, true);
	DWORD* dw_ptr = (DWORD*)tmpBuf.getContent();
	size_t dw_size = tmpBuf.getContentSize() / sizeof(DWORD);
	for (int i = 0; i < dw_size; i++) {
		dw_ptr[i] ^= sign->checksum;
	}
	QCryptographicHash calcHash(QCryptographicHash::Md5);
	calcHash.addData((char*) tmpBuf.getContent(), tmpBuf.getContentSize());
	return QString(calcHash.result().toHex());
}

void CalcThread::run()
{
	QMutexLocker lock(&myMutex);
	if (this->isStopRequested()) {
		return;
	}
	QString fileHash = "Cannot calculate!";
	if (!m_PE || !m_PE->getContent()) {
		emit gotHash(fileHash, hashType);
		return;
	}
	QCryptographicHash::Algorithm qHashType = QCryptographicHash::Md5;
	if (hashType == SupportedHashes::MD5) {
		qHashType = QCryptographicHash::Md5;
	} else if (hashType == SupportedHashes::SHA1) {
		qHashType = QCryptographicHash::Sha1;
	} 
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0) //the feature was introduced in Qt5.0
	else if (hashType == SupportedHashes::SHA256) {
		qHashType = QCryptographicHash::Sha256;
	}
#endif
	emit gotHash("Calculating...", hashType);
	try {
		if (hashType == SupportedHashes::CHECKSUM) {
			long checksum = PEFile::computeChecksum((BYTE*) m_PE->getContent(), m_PE->getContentSize(), checksumOff);
			fileHash = QString::number(checksum, 16);
		}
		else if (hashType == SupportedHashes::RICH_HDR_MD5) {
			fileHash = makeRichHdrHash();
		}
		else if (hashType == SupportedHashes::IMP_MD5) {
			fileHash = makeImpHash();
		}
		else {
			QCryptographicHash calcHash(qHashType);
			calcHash.addData((char*) m_PE->getContent(), m_PE->getContentSize());
			fileHash = QString(calcHash.result().toHex());
		}
	} catch (...) {
		fileHash = "Cannot calculate!";
	}
	emit gotHash(fileHash, hashType);
}

