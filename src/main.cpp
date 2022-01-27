#define OLC_PGE_APPLICATION
#include <olcPixelGameEngine.h>
#define OLC_PGEX_SOUND
#include <Extensions/olcPGEX_Sound.h>

#define TARA_PGE_EXTENSION
#include <taraNS\mf.hpp>

#include <iostream>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// timestamp
struct TS {
	int8_t sec;
	int min;

	TS(int secs) {
		sec = secs % 60;
		min = secs / 60;
	}
	std::string str() {
		std::string out = "";
		for (int i = 0; i < 2 - (int)tara::mf::len(min); i++) out += "0";
		out += std::to_string(min) + ":";
		for (int i = 0; i < 2 - (int)tara::mf::len(sec); i++) out += "0";
		out += std::to_string(sec);
		return out;
	}
};
struct Row {
	std::string sName;
	int nId;
	float fLen;

	Row(): sName(""), nId(0), fLen(0.0f) {}
	Row(std::string sName_, int nId_, float fLen_) : nId(nId_), fLen(fLen_){
		sName = tara::mf::pySplit(sName_, "\\")[1];
		sName = tara::mf::pySplit(sName, ".")[0];
	}
	void draw(olc::PixelGameEngine* pge, olc::vi2d pos, olc::Pixel bg = olc::BLACK, bool bSelected = false) {
		if (sName == "") return;
		olc::vi2d newPos = pos + (nId ) * olc::vi2d{ 0, 16 + 8 };
		pge->FillRect(newPos, { pge->ScreenWidth(), 16 + 4 }, bg);
		if (bSelected) pge->DrawRect(newPos, { pge->ScreenWidth(), 16 + 4}, olc::RED);
		constexpr int nTextOff = 2;
		pge->DrawString(newPos + olc::vi2d{5,nTextOff }, /*std::to_string(nId) + ": " + */ sName, olc::WHITE, 2);
		newPos += olc::vi2d{ pge->ScreenWidth() - 16 * (int)TS((int)fLen).str().size(), nTextOff};
		pge->DrawString(newPos, 
			TS((int)fLen).str(), olc::WHITE, 2);
	}
};

class Queuer : public olc::PixelGameEngine
{
public:
	Queuer() {
		sAppName = "Queuer";
	}


	olc::SOUND s;
private:
	std::vector<Row> vRows;

	int nCurrSound = 0, nSelect;

	// return if successfull
	bool AddTrack(std::string sName) {
		int id = s.LoadAudioSample(sName);
		if (id == -1) return false;

		auto WaveLen = [&](std::string sFileName)
		{
			std::ifstream is(sFileName, std::ifstream::binary);
			char dump[4];
			is.read(dump, sizeof(char) * 4); // Read "RIFF"
			is.read(dump, sizeof(char) * 4); // Not Interested
			is.read(dump, sizeof(char) * 4); // Read "WAVE"
			// Read Wave description chunk
			is.read(dump, sizeof(char) * 4); // Read "fmt "
			unsigned int nHeaderSize = 0;
			is.read((char*)&nHeaderSize, sizeof(unsigned int)); // Not Interested
			OLC_WAVEFORMATEX wavHeader;
			is.read((char*)&wavHeader, nHeaderSize);// sizeof(WAVEFORMATEX)); // Read Wave Format Structure chunk
														// Note the -2, because the structure has 2 bytes to indicate its own size
														// which are not in the wav file

			// Search for audio data chunk
			uint32_t nChunksize = 0;
			is.read(dump, sizeof(char) * 4); // Read chunk header
			is.read((char*)&nChunksize, sizeof(uint32_t)); // Read chunk size
			while (strncmp(dump, "data", 4) != 0)
			{
				// Not audio data, so just skip it
				//std::fseek(f, nChunksize, SEEK_CUR);
				is.seekg(nChunksize, std::istream::cur);
				is.read(dump, sizeof(char) * 4);
				is.read((char*)&nChunksize, sizeof(uint32_t));
			}
			is.close();

			// Finally got to data, so read it all in and convert to float samples
			int32_t nSamples = nChunksize / (wavHeader.nChannels * (wavHeader.wBitsPerSample >> 3));
			return (float)nSamples / 44100.0f;
		};

		while (vRows.size() < id) vRows.push_back({});
		vRows.insert(vRows.begin() + id, { sName, id , WaveLen(sName)});
	}
	
protected:
	bool OnUserCreate() override {
		s.InitialiseAudio();

		//for (int i=0; i< 10; i++)
		//	AddTrack("04 investigation.wav");

		for (const auto& entry : fs::directory_iterator("lineup"))
			AddTrack(entry.path().string());
		//s.listActiveSamples
		return true;
	}

	bool OnUserUpdate(float elapsedTime) override {
		Sleep(1000.0f * std::max(0.0f, (1.0f / 25.0f) - elapsedTime));
		if (GetKey(olc::ESCAPE).bPressed) return false;
		
		Clear(olc::BLACK);
		if (GetKey(olc::UP).bPressed)   nSelect = std::max(1, nSelect - 1);
		if (GetKey(olc::DOWN).bPressed) nSelect = std::min((int)vRows.size() - 1, nSelect + 1);


		for (int i = 1; i < vRows.size(); i++) {
			vRows[i].draw(this, { 0,0 }, (nCurrSound == i) ? olc::GREEN : olc::DARK_GREY, i == nSelect);
		}
		if (GetKey(olc::ENTER).bPressed) {
			s.StopAll();
			s.listActiveSamples.clear();
			nCurrSound = nSelect;
			s.PlaySample(nCurrSound);
		}
		if (GetKey(olc::SPACE).bPressed) { 
			if (s.listActiveSamples.size() > 0) {
				s.StopAll();
				s.listActiveSamples.clear();
				nCurrSound = 0;
			}
			else {
				s.PlaySample(nSelect);
				nCurrSound = nSelect;
			}
		}

		if (s.listActiveSamples.size() > 0) {
			int id = s.listActiveSamples.begin()->nAudioSampleID;
			DrawString({ 20, ScreenHeight() - 100 }, /*std::to_string(id) + ": " +*/ vRows[id].sName, olc::YELLOW, 3); // size 8 << 3 == 64px

			DrawString({ 20, ScreenHeight() - 36 }, TS(s.listActiveSamples.begin()->nSamplePosition / 44100).str() + " / " + TS((int)vRows[id].fLen).str(), olc::YELLOW, 2);


		}
		else {
			s.StopAll();
			s.listActiveSamples.clear();
			nCurrSound = 0;
		}

		return true;
	}
};

int main() {
	constexpr uint8_t nRes = 2;
	Queuer demo;
	if (demo.Construct(256 << nRes, 240 << nRes, 4 >> nRes, 4 >> nRes))
		demo.Start();
	demo.s.DestroyAudio();
	return 0;
}