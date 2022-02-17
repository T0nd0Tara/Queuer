#define OLC_PGE_APPLICATION
#include <olcPixelGameEngine.h>
#define OLC_PGEX_SOUND
#include <Extensions/olcPGEX_Sound_EXTERNAL.h>

#define TARA_PGE_EXTENSION
#include <taraNS\mf.hpp>
#include <taraNS\Bools.hpp>

#include <iostream>
#include <vector>
#include <filesystem>

#define len tara::mf::len
int TEXT_SIZE = 2;
#define ROW_SIZE (TEXT_SIZE + 1) * 8

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
		for (int i = 0; i < 2 - (int)len(min); i++) out += "0";
		out += std::to_string(min) + ":";
		for (int i = 0; i < 2 - (int)len(sec); i++) out += "0";
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
		olc::vi2d newPos = pos + (nId ) * olc::vi2d{ 0, ROW_SIZE };
		pge->FillRect(newPos, { pge->ScreenWidth(), 8 * TEXT_SIZE + 4 }, bg);
		if (bSelected) pge->DrawRect(newPos, { pge->ScreenWidth(), 8 * TEXT_SIZE + 4}, olc::RED);
		constexpr int nTextOff = 2;
		pge->DrawString(newPos + olc::vi2d{5,nTextOff }, /*std::to_string(nId) + ": " + */ sName, olc::WHITE, TEXT_SIZE);
		newPos += olc::vi2d{ pge->ScreenWidth() - TEXT_SIZE * 8 * (int)TS((int)fLen).str().size() - pos.x, nTextOff};
		pge->DrawString(newPos, 
			TS((int)fLen).str(), olc::WHITE, TEXT_SIZE);
	}
};

class Queuer : public olc::PixelGameEngine
{
public:
	Queuer() {
		sAppName = "Queuer: ";
		std::ifstream f("lineup/title.txt");
		if (f.is_open()) {
			std::getline(f, title);
			f.close();

			sAppName += title;
		}
	}


	olc::SOUND s;
private:
	std::vector<Row> vRows;

	int
		nCurrSound1 = 0,
		nCurrSound2 = 0,
		nSelect;
	int nLastPlayed[2] = { 0 , 0 };
	
	//float fPausePt = 0.0f;
	std::string title;

	void UpdateNLast() {
		nLastPlayed[1] = nLastPlayed[0];
		nLastPlayed[0] = nSelect;
	}

	// return if successfull
	bool AddTrack(std::string sName) {
		int id = s.LoadAudioSample(sName);
		if (id == -1) return false;

		auto WaveLen = [&](std::string sFileName)
		{
			std::ifstream is(sFileName, std::ifstream::binary);
			char dump[4];
			constexpr uint64_t dump_size = sizeof(char) * 4;
			is.read(dump, dump_size); // Read "RIFF"
			is.read(dump, dump_size); // Not Interested
			is.read(dump, dump_size); // Read "WAVE"
			// Read Wave description chunk
			is.read(dump, dump_size); // Read "fmt "
			unsigned int nHeaderSize = 0;
			is.read((char*)&nHeaderSize, sizeof(unsigned int)); // Not Interested
			OLC_WAVEFORMATEX wavHeader;
			is.read((char*)&wavHeader, nHeaderSize);// sizeof(WAVEFORMATEX)); // Read Wave Format Structure chunk
														// Note the -2, because the structure has 2 bytes to indicate its own size
														// which are not in the wav file

			// Search for audio data chunk
			uint32_t nChunksize = 0;
			is.read(dump, dump_size); // Read chunk header
			is.read((char*)&nChunksize, sizeof(uint32_t)); // Read chunk size
			while (strncmp(dump, "data", 4) != 0)
			{
				// Not audio data, so just skip it
				is.seekg(nChunksize, std::istream::cur);
				is.read(dump, dump_size);
				is.read((char*)&nChunksize, sizeof(uint32_t));
			}
			is.close();

			// Finally got to data, so read it all in and convert to float samples
			int32_t nSamples = nChunksize / (wavHeader.nChannels * (wavHeader.wBitsPerSample >> 3));
			return (float)nSamples / 44100.0f;
		};

		while (vRows.size() < id) vRows.push_back({});
		vRows.insert(vRows.begin() + id, { sName, id , WaveLen(sName)});
		return true;
	}
	
protected:
	bool OnUserCreate() override {
		s.InitialiseAudio();

		for (const auto& entry : std::filesystem::directory_iterator("lineup"))
			AddTrack(entry.path().string());
		if (vRows.size() > 24) TEXT_SIZE = 1;
		return true;
	}

	bool OnUserUpdate(float elapsedTime) override {
		Sleep(1000.0f * std::max(0.0f, (1.0f / 25.0f) - elapsedTime));

		Clear(olc::BLACK);
		if (GetKey(olc::UP).bPressed)   nSelect = std::max(1, nSelect - 1);
		if (GetKey(olc::DOWN).bPressed) nSelect = std::min((int)vRows.size() - 1, nSelect + 1);

		DrawString({ ROW_SIZE,0 }, title, olc::YELLOW, TEXT_SIZE);
		for (int i = 1; i < vRows.size(); i++) {
			olc::Pixel col = olc::DARK_GREY;
			if (nCurrSound1 == i) col = olc::GREEN;
			if (nCurrSound2 == i) col = olc::DARK_GREEN;
			vRows[i].draw(this, { ROW_SIZE,0 }, col, i == nSelect);
		}
		// Red Box before selected track
		if (nSelect) {
			FillRect({ 0, nSelect * ROW_SIZE }, { ROW_SIZE,ROW_SIZE }, olc::RED);
		}
		if (GetKey(olc::ENTER).bPressed) {
			UpdateNLast();
			if (nSelect == nCurrSound2) {
				s.StopAll();
				nCurrSound2 = 0;
			}
			else s.StopSample(nCurrSound1);

			nCurrSound1 = nSelect;
			s.PlaySample(nCurrSound1);
		}
		if (GetKey(olc::SPACE).bPressed) {

			bool bStop = false;
			for (auto it = s.listActiveSamples.begin(); it != s.listActiveSamples.end(); it++)
				if (it->nAudioSampleID == nCurrSound1) {
					bStop = true;
					break;
				}

			if (bStop) {
				s.StopSample(nCurrSound1);
				nCurrSound1 = 0;
			}
			else {
				UpdateNLast();
				if (nSelect == nCurrSound2) {
					s.StopAll();
					nCurrSound2 = 0;
				}
				s.PlaySample(nSelect);
				nCurrSound1 = nSelect;
			}
		}
		if (GetKey(olc::K2).bPressed) {
			bool bStop = false;
			for (auto it = s.listActiveSamples.begin(); it != s.listActiveSamples.end(); it++)
				if (it->nAudioSampleID == nCurrSound2) {
					bStop = true;
					break;
				}
			if (bStop) {
				s.StopSample(nCurrSound2);
				nCurrSound2 = 0;
			}
			else if (nSelect != nCurrSound1) {
				UpdateNLast();
				s.PlaySample(nSelect);
				nCurrSound2 = nSelect;
			}
		}
		if (GetKey(olc::P).bPressed) {
			s.StopAll();
			s.listActiveSamples.clear();
			nCurrSound1 = 0;
			nCurrSound2 = 0;
		}


		if (GetKey(olc::Z).bPressed && nLastPlayed[0] != 0) {
			nSelect = nLastPlayed[0];
			nLastPlayed[0] = nLastPlayed[1];
			nLastPlayed[1] = 0;
		}
		if (nCurrSound1 != 0) {
			std::list<olc::SOUND::sCurrentlyPlayingSample>::iterator sound;
			for (sound = s.listActiveSamples.begin(); sound != s.listActiveSamples.end(); sound++)
				if (sound->nAudioSampleID == nCurrSound1) {
					break;
				}

			DrawString({ 20, ScreenHeight() - 100 }, vRows[nCurrSound1].sName, olc::YELLOW, TEXT_SIZE + 1); // size 8 << 3 == 64px

			DrawString({ 20, ScreenHeight() - 36 }, TS(sound->nSamplePosition / 44100).str() + " / " + TS((int)vRows[nCurrSound1].fLen).str(), olc::YELLOW, TEXT_SIZE);

		}
		if (nCurrSound2 != 0) {
			std::list<olc::SOUND::sCurrentlyPlayingSample>::iterator sound;
			for (sound = s.listActiveSamples.begin(); sound != s.listActiveSamples.end(); sound++)
				if (sound->nAudioSampleID == nCurrSound2) {
					break;
				}

			FillRect({ ScreenWidth() >> 1, ScreenHeight() - 100 }, { ScreenWidth() >> 1 , 100 }, olc::BLACK);

			DrawString({ 20 + (ScreenWidth() >> 1), ScreenHeight() - 100 }, vRows[nCurrSound2].sName, olc::YELLOW, TEXT_SIZE + 1); // size 8 << 3 == 64px

			DrawString({ 20 + (ScreenWidth() >> 1), ScreenHeight() - 36 }, TS(sound->nSamplePosition / 44100).str() + " / " + TS((int)vRows[nCurrSound2].fLen).str(), olc::YELLOW, TEXT_SIZE);

		}
		// deleting nCurrSound's
		tara::Bools bDel = 0xFF;
		for (auto& sound : s.listActiveSamples) {
			if (nCurrSound1 == sound.nAudioSampleID /*&&
				fPausePt != 0.0f*/)					 bDel.set_val(1, false);
			if (nCurrSound2 == sound.nAudioSampleID) bDel.set_val(2, false);
		}
		if (bDel[1]) nCurrSound1 = 0;
		if (bDel[2]) nCurrSound2 = 0;



		return true;
	}
};

int main() {

	Queuer demo;
	if (demo.Construct(720, 720, 1, 1, false, true))
		demo.Start();
	demo.s.DestroyAudio();

	return 0;
}