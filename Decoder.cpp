#include "Decoder.h"
#include "pnacl_player.h"

namespace PnaclPlayer
{
	Decoder::Decoder(pnacl_player* instance, int id, const pp::Graphics3D& graphics_3d, int hwaccel) : instance_(instance), id_(id), ppDecoder(new pp::VideoDecoder(instance)), callback_factory_(this), next_picture_id_(0), flushing_(false), resetting_(false), initializing_(true), decode_looping_(false), total_latency_(0.0), num_pictures_(0), currentStreamNum(0)
	{
		core_if_ = static_cast<const PPB_Core*>(pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));

		const PP_VideoProfile kBitstreamProfile = PP_VIDEOPROFILE_H264HIGH;

		assert(!ppDecoder->is_null());
		PP_HardwareAcceleration hwva = PP_HARDWAREACCELERATION_NONE;
		if (hwaccel == 0)
		{
			hwva = PP_HARDWAREACCELERATION_NONE;
			instance->PostString("PP_HARDWAREACCELERATION_NONE");
		}
		else if (hwaccel == 1)
		{
			hwva = PP_HARDWAREACCELERATION_WITHFALLBACK;
			instance->PostString("PP_HARDWAREACCELERATION_WITHFALLBACK");
		}
		else if (hwaccel == 2)
		{
			hwva = PP_HARDWAREACCELERATION_ONLY;
			instance->PostString("PP_HARDWAREACCELERATION_ONLY");
		}
		ppDecoder->Initialize(graphics_3d, kBitstreamProfile, hwva, 0, callback_factory_.NewCallback(&Decoder::InitializeDone));
	}

	Decoder::~Decoder()
	{
		delete ppDecoder;
	}
	void Decoder::InitializeDone(int32_t result)
	{
		assert(ppDecoder);
		assert(result == PP_OK);
		Start();
	}

	void Decoder::Start()
	{
		assert(ppDecoder);

		next_picture_id_ = 0;

		// Register callback to get the first picture. We call GetPicture again in
		// PictureReady to continuously receive pictures as they're decoded.
		ppDecoder->GetPicture(callback_factory_.NewCallbackWithOutput(&Decoder::PictureReady));

		// Start the decode loop.
		if (initializing_)
			instance_->PostString("initialized");
		initializing_ = false;
		DecodeNextFrame();
	}

	void Decoder::Reset()
	{
		assert(ppDecoder);
		if (resetting_ || initializing_)
			return;
		resetting_ = true;
		while (DeleteFirstFrameInQueue());
		ppDecoder->Reset(callback_factory_.NewCallback(&Decoder::ResetDone));
	}

	void Decoder::ResetDone(int32_t result)
	{
		assert(ppDecoder);
		assert(result == PP_OK);
		assert(resetting_);
		resetting_ = false;

		Start();
	}

	void Decoder::RecyclePicture(const PP_VideoPicture& picture)
	{
		assert(ppDecoder);
		ppDecoder->RecyclePicture(picture);
	}

	void Decoder::ReceiveFrame(pp::VarArrayBuffer& buffer, long long timestamp)
	{
		encodedFrameQueue.push(EncodedFrame(buffer, timestamp));
		if (!resetting_ && !flushing_ && !initializing_ && !decode_looping_)
			DecodeNextFrame();
	}

	void Decoder::DecodeNextFrame()
	{
		assert(ppDecoder);
		// TODO: 
		// If we've just reached the end of the bitstream, flush and wait.
		//if (!flushing_ && encoded_data_next_pos_to_decode_ == kDataLen)
		//{
		//	flushing_ = true;
		//	ppDecoder->Flush(callback_factory_.NewCallback(&Decoder::FlushDone));
		//	return;
		//}
		if (encodedFrameQueue.empty())
		{
			decode_looping_ = false;
			return; // No frame is currently available.  Exit the frame queue
		}
		decode_looping_ = true;

		// Decode the frame. On completion, DecodeDone will call DecodeNextFrame to implement a decode loop.
		currentlyDecodingFrame = encodedFrameQueue.front();
		encodedFrameQueue.pop();
		decode_time_[next_picture_id_ % kMaxDecodeDelay] = core_if_->GetTimeTicks();
		ppDecoder->Decode(next_picture_id_++, currentlyDecodingFrame.buffer.ByteLength(), currentlyDecodingFrame.buffer.Map(), callback_factory_.NewCallback(&Decoder::DecodeDone));
	}

	void Decoder::DecodeDone(int32_t result)
	{
		assert(ppDecoder);

		// Break out of the decode loop on abort.
		if (result == PP_ERROR_ABORTED)
		{
			decode_looping_ = false;
			return;
		}
		assert(result == PP_OK);
		if (!flushing_ && !resetting_)
			DecodeNextFrame();
	}

	void Decoder::PictureReady(int32_t result, PP_VideoPicture picture)
	{
		assert(ppDecoder);
		// Break out of the get picture loop on abort.
		if (result == PP_ERROR_ABORTED)
			return;
		assert(result == PP_OK);
		num_pictures_++;
		PP_TimeTicks currentTime = core_if_->GetTimeTicks();
		PP_TimeTicks latency = currentTime - decode_time_[picture.decode_id % kMaxDecodeDelay];
		total_latency_ += latency;

		ppDecoder->GetPicture(callback_factory_.NewCallbackWithOutput(&Decoder::PictureReady));

		instance_->ReceiveDecodedPicture(DecodedFrame(this, picture, currentStreamNum, 0));
	}

	void Decoder::FlushDone(int32_t result)
	{
		assert(ppDecoder);
		assert(result == PP_OK || result == PP_ERROR_ABORTED);
		assert(flushing_);
		flushing_ = false;
	}
}