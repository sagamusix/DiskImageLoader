#include "DiskLoader.h"
#include "Windows.h"
#include <QElapsedTimer>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <bitset>
#include <string>

DiskLoader::DiskLoader(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	setFixedSize(size());
	setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);

	// Open configuration file
	QFile configFile(QCoreApplication::applicationDirPath() + "/config.json");
	QJsonDocument json;
	QJsonObject obj;
	if(configFile.open(QIODevice::ReadOnly))
	{
		json = QJsonDocument::fromJson(configFile.readAll());
		if(json.isObject())
		{
			obj = json.object();
		}
	}
	std::wstring defaultVolume;
	if(obj["default_volume"].isString())
	{
		defaultVolume = obj["default_volume"].toString().toStdWString();
	}

	// List all removable volumes
	// https://stackoverflow.com/questions/327718/how-to-list-physical-disks
	std::bitset<32> drives(GetLogicalDrives());
	for(char drive = 'A'; drive <= 'Z'; drive++)
	{
		std::wstring driveStr = static_cast<wchar_t>(drive) + std::wstring(L":\\");
		if(drives[drive - 'A'] && GetDriveType(driveStr.c_str()) == DRIVE_REMOVABLE)
		{
			WCHAR volumeName[256] = { 0 };
			GetVolumeInformation(driveStr.c_str(), volumeName, std::size(volumeName), nullptr, nullptr, nullptr, nullptr, 0);
			driveStr += std::wstring(L" (") + volumeName + L")";

			HANDLE h = CreateFileW((std::wstring(L"\\\\.\\") + static_cast<wchar_t>(drive) + L":").c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
				OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS, NULL);
			if(h == INVALID_HANDLE_VALUE)
			{
				continue;
			}
			DWORD bytesReturned;
			VOLUME_DISK_EXTENTS vde;
			if(!DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &vde, sizeof(vde), &bytesReturned, NULL))
			{
				continue;
			}

			if(vde.NumberOfDiskExtents == 1)
			{
				ui.driveBox->addItem(QString::fromStdWString(driveStr), QVariant::fromValue(vde.Extents[0].DiskNumber));
				if(volumeName == defaultVolume)
				{
					ui.driveBox->setCurrentIndex(ui.driveBox->count() - 1);
				}
			} else
			{
				// This setup doesn't make sense for our use case
				continue;
			}
		}
	}

	ui.offsetEdit1->setText("3655680");
	ui.offsetEdit2->setText("5007360");
	ui.offsetEdit3->setText("6359040");

	if(!obj.isEmpty())
	{
		if(obj["offset1"].isString())
			ui.offsetEdit1->setText(obj["offset1"].toString());
		if(obj["offset2"].isString())
			ui.offsetEdit2->setText(obj["offset2"].toString());
		if(obj["offset3"].isString())
			ui.offsetEdit3->setText(obj["offset3"].toString());
	}
	
	connect(ui.fileBrowse1, SIGNAL(clicked()), this, SLOT(OnBrowse1()));
	connect(ui.fileBrowse2, SIGNAL(clicked()), this, SLOT(OnBrowse2()));
	connect(ui.fileBrowse3, SIGNAL(clicked()), this, SLOT(OnBrowse3()));

	connect(ui.copyButton, SIGNAL(clicked()), this, SLOT(OnStart()));
}


void DiskLoader::OnBrowse(QLineEdit *edit)
{
	QString fileName = edit->text();
	fileName = QFileDialog::getOpenFileName(this, tr("Open Disk Image"), fileName, tr("Raw Disk Images (*.iso)"));
	if(!fileName.isEmpty())
	{
		edit->setText(fileName);
	}
}


static uint64_t GetFileSize(HANDLE h)
{
	DWORD high = 0;
	DWORD low = GetFileSize(h, &high);
	return (static_cast<uint64_t>(high) << 32) | low;
}


void DiskLoader::OnStart()
{
	if(ui.driveBox->currentIndex() == -1)
	{
		QMessageBox::warning(this, tr("Drive Selection"), tr("No target drive has been selected!"));
		return;
	}

	int diskNumber = ui.driveBox->currentData(Qt::UserRole).toInt();
	WCHAR s[64];
	swprintf(s, L"\\\\?\\PhysicalDrive%d", diskNumber);
	HANDLE hDrive = CreateFileW(s, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS, NULL);
	if(hDrive == INVALID_HANDLE_VALUE)
	{
		QMessageBox::warning(this, tr("Drive Selection"), tr("Selected drive cannot be opened!"));
		return;
	}
	uint64_t driveSize = GetFileSize(hDrive);

	QLineEdit *fileNames[] = { ui.fileEdit1, ui.fileEdit2, ui.fileEdit3 };
	QLineEdit *offsets[] = { ui.offsetEdit1, ui.offsetEdit2, ui.offsetEdit3 };

	for(int disk = 0; disk < 3; disk++)
	{
		QString filename = fileNames[disk]->text();
		uint64_t offset = offsets[disk]->text().toLongLong() * 512;
		if(filename.isEmpty())
		{
			continue;
		}

		LONG seekLow = static_cast<LONG>(offset & 0xFFFFFFFF), seekHigh = static_cast<LONG>(offset >> 32);
		DWORD result = SetFilePointer(hDrive, seekLow, &seekHigh, FILE_BEGIN);
		if(result == INVALID_SET_FILE_POINTER)
		{
			QMessageBox::warning(this, tr("Drive Selection"), tr("Cannot seek to offset!"));
			continue;
		}

		HANDLE hSrc = CreateFileW(filename.toStdWString().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS, NULL);
		if(hSrc == INVALID_HANDLE_VALUE)
		{
			QMessageBox::warning(this, tr("Drive Selection"), QString("Cannot open") + filename + QString(" for reading!"));
			continue;
		}

		uint64_t fileSize = GetFileSize(hSrc);
		if(offset + fileSize > driveSize)
		{
			QMessageBox::warning(this, tr("Copy File"), filename + QString(" is too big!"));
			continue;
		}

		ui.progressBar->setMaximum(fileSize >> 10);

		QElapsedTimer timer;
		timer.start();

		uint64_t bytesLeft = fileSize;
		while(bytesLeft > 0)
		{
			char buffer[65536];
			DWORD read = 0, written = 0;
			ReadFile(hSrc, buffer, sizeof(buffer), &read, nullptr);
			WriteFile(hDrive, buffer, read, &written, nullptr);
			if(read != written)
			{
				QMessageBox::warning(this, tr("Read != Written"), tr("Write failed?"));
				break;
			}
			bytesLeft -= read;

			if(timer.hasExpired(100))
			{
				ui.progressBar->setValue((fileSize - bytesLeft) >> 10);
				QCoreApplication::processEvents();
				timer.start();
			}
		}

		CloseHandle(hSrc);
	}
	CloseHandle(hDrive);
}
