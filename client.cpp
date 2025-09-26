/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Ethan Rendell
	UIN: 432004004
	Date: Sept 16, 2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;

	int buffercapacity = MAX_MESSAGE;
	bool new_chan = false;

	vector<FIFORequestChannel*> channels;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				buffercapacity = atoi(optarg);
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}

	pid_t server_split = fork();

    if (server_split == 0) {
		char *args[] = {(char *)"./server", (char*)"-m", (char*)std::to_string(buffercapacity).c_str(), NULL};
        execvp(args[0], args);
    }


    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&cont_chan);

	if (new_chan) {
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
    	cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));

		char buf0[1024];
		cont_chan.cread(buf0, sizeof(buf0));

		FIFORequestChannel* newChan = new FIFORequestChannel(buf0, FIFORequestChannel::CLIENT_SIDE);
		channels.push_back(newChan);
	}

	FIFORequestChannel chan = *(channels.back());

	if (p != -1 && t != -1 && e != -1) {
		char buf[MAX_MESSAGE]; // 256
		datamsg x(p, t, e);
		
		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan.cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	} else if (p != -1) {
		// Open file
		ofstream file;
		filename = "received/x1.csv";
		file.open(filename);

		// Since t is not declared, make variable
		double t_val = 0.0;
		int count = 0;

		while (count < 1000) {
			file << t_val;
			// i is basically e, e is either 1 or 2
			for (int i = 1; i <= 2; i++) {
				char buf[MAX_MESSAGE];
				datamsg x(p, t_val, i);

				memcpy(buf, &x, sizeof(datamsg));
				chan.cwrite(buf, sizeof(datamsg)); // question
				double reply;
				chan.cread(&reply, sizeof(double)); //answer
				file << "," << reply;
			}
			
			file << "\n";
			t_val += 0.004; // t increases by 0.004
			count++;
		}
		file.close();
	} else if (filename != "") {
		filemsg fm(0, 0);
		string fname = filename;

		int len = sizeof(filemsg) + (fname.size() + 1);

		char* buf2 = new char[len]; // buffer request
		char* buf3 = new char [buffercapacity]; // buffer response

		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan.cwrite(buf2, len);

		__int64_t filesize;
		chan.cread(&filesize, sizeof(__int64_t));

		cerr << "File size reported by server: " << filesize << " bytes" << endl;

		ofstream file;
		file.open("received/" + fname, ios::binary);

		__int64_t segments_count = floor((double)filesize / (double)buffercapacity);
		__int64_t remainder = filesize % buffercapacity;
		__int64_t total_bytes = 0;

		for (__int64_t i = 0; i < segments_count; i++) {
			filemsg* file_req = (filemsg*)buf2;
			file_req->offset = buffercapacity * i;
			file_req->length = buffercapacity;

			total_bytes += file_req->length;

			chan.cwrite(buf2, len);

			chan.cread(buf3, file_req->length);

			file.write(buf3, file_req->length);
		}

		// LAST Segment
		if (remainder > 0) {
			filemsg* file_req = (filemsg*)buf2;
			file_req->offset = buffercapacity * segments_count;
			file_req->length = remainder;

			total_bytes += file_req->length;

			chan.cwrite(buf2, len);

			chan.cread(buf3, file_req->length);

			file.write(buf3, file_req->length);
		}


		cerr << "Total Bytes/" << total_bytes << endl;

		file.close();
		
		delete[] buf2;
		delete[] buf3;

		cerr << "File transfer complete: received/" << fname << endl;
	}

	if (new_chan) {
		// deletes
		MESSAGE_TYPE del = QUIT_MSG;
    	chan.cwrite(&del, sizeof(MESSAGE_TYPE));

		delete channels.back();
		chan = *(channels.front());
	}

    // closing the control channel
    MESSAGE_TYPE m = QUIT_MSG;
    chan.cwrite(&m, sizeof(MESSAGE_TYPE));

    // IMPORTANT: do NOT delete cont_chan, since it's on the stack
    channels.clear();

    // wait for server to terminate
    int status;
    waitpid(server_split, &status, 0);

}