#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_DiskLoader.h"

class DiskLoader : public QMainWindow
{
	Q_OBJECT

public:
	DiskLoader(QWidget *parent = Q_NULLPTR);

private slots:

	void OnBrowse1() { OnBrowse(ui.fileEdit1); }
	void OnBrowse2() { OnBrowse(ui.fileEdit2); }
	void OnBrowse3() { OnBrowse(ui.fileEdit3); }
	void OnStart();

private:
	void OnBrowse(QLineEdit *edit);

	Ui::DiskLoaderClass ui;
};
