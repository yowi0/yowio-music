#include "downloader.h"

Downloader::Downloader(std::string url, std::string downProg = "yt-dlp")
{
	this->downloadUrl = url;
	this->downloaderProgram = downProg;
}

int Downloader::quickDownload()
{
	static int callCounter = 0;

	std::string cmd = this->downloaderProgram + " " + "--extract-audio --audio-format mp3 --restrict-filenames -o songs/\"%(title)s\" " + this->downloadUrl;

	std::cout << "Downloading using the " << callCounter + 1 << "th downloader object" << std::endl;
	int status = system(cmd.c_str());
	callCounter++;

	return status;
}