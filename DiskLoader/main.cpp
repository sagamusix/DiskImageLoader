#include "DiskLoader.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	DiskLoader w;
	w.show();
	return a.exec();
}
