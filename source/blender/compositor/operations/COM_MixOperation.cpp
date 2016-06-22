/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_MixOperation.h"

extern "C" {
#  include "BLI_math.h"
#  include "BLI_math_color_blend.h"
}

/* ******** Mix Base Operation ******** */

MixBaseOperation::MixBaseOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_inputValueOperation = NULL;
	this->m_inputColor1Operation = NULL;
	this->m_inputColor2Operation = NULL;
	this->setUseValueAlphaMultiply(false);
	this->setUseClamp(false);
}

void MixBaseOperation::initExecution()
{
	this->m_inputValueOperation = this->getInputSocketReader(0);
	this->m_inputColor1Operation = this->getInputSocketReader(1);
	this->m_inputColor2Operation = this->getInputSocketReader(2);
}

void MixBaseOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	
	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);
	
	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_mix_float_v4(output, inputColor1, inputColor2, value);
}

void MixBaseOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	NodeOperationInput *socket;
	unsigned int tempPreferredResolution[2] = {0, 0};
	unsigned int tempResolution[2];

	socket = this->getInputSocket(1);
	socket->determineResolution(tempResolution, tempPreferredResolution);
	if ((tempResolution[0] != 0) && (tempResolution[1] != 0)) {
		this->setResolutionInputSocketIndex(1);
	}
	else {
		socket = this->getInputSocket(2);
		socket->determineResolution(tempResolution, tempPreferredResolution);
		if ((tempResolution[0] != 0) && (tempResolution[1] != 0)) {
			this->setResolutionInputSocketIndex(2);
		}
		else {
			this->setResolutionInputSocketIndex(0);
		}
	}
	NodeOperation::determineResolution(resolution, preferredResolution);
}

void MixBaseOperation::deinitExecution()
{
	this->m_inputValueOperation = NULL;
	this->m_inputColor1Operation = NULL;
	this->m_inputColor2Operation = NULL;
}

/* ******** Mix Add Operation ******** */

MixAddOperation::MixAddOperation() : MixBaseOperation()
{
	/* pass */
}

void MixAddOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_add_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Blend Operation ******** */

MixBlendOperation::MixBlendOperation() : MixBaseOperation()
{
	/* pass */
}

void MixBlendOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	float value;

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);
	value = inputValue[0];
	
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_mix_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Burn Operation ******** */

MixBurnOperation::MixBurnOperation() : MixBaseOperation()
{
	/* pass */
}

void MixBurnOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_burn_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Color Operation ******** */

MixColorOperation::MixColorOperation() : MixBaseOperation()
{
	/* pass */
}

void MixColorOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_color_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Darken Operation ******** */

MixDarkenOperation::MixDarkenOperation() : MixBaseOperation()
{
	/* pass */
}

void MixDarkenOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_darken_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Difference Operation ******** */

MixDifferenceOperation::MixDifferenceOperation() : MixBaseOperation()
{
	/* pass */
}

void MixDifferenceOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_difference_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Difference Operation ******** */

MixDivideOperation::MixDivideOperation() : MixBaseOperation()
{
	/* pass */
}

void MixDivideOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_divide_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Dodge Operation ******** */

MixDodgeOperation::MixDodgeOperation() : MixBaseOperation()
{
	/* pass */
}

void MixDodgeOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_dodge_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Glare Operation ******** */

MixGlareOperation::MixGlareOperation() : MixBaseOperation()
{
	/* pass */
}

void MixGlareOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	float value;

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);
	value = inputValue[0];
	float mf = 2.0f - 2.0f * fabsf(value - 0.5f);

	if (inputColor1[0] < 0.0f) inputColor1[0] = 0.0f;
	if (inputColor1[1] < 0.0f) inputColor1[1] = 0.0f;
	if (inputColor1[2] < 0.0f) inputColor1[2] = 0.0f;

	output[0] = mf * max(inputColor1[0] + value * (inputColor2[0] - inputColor1[0]), 0.0f);
	output[1] = mf * max(inputColor1[1] + value * (inputColor2[1] - inputColor1[1]), 0.0f);
	output[2] = mf * max(inputColor1[2] + value * (inputColor2[2] - inputColor1[2]), 0.0f);
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Hue Operation ******** */

MixHueOperation::MixHueOperation() : MixBaseOperation()
{
	/* pass */
}

void MixHueOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_hue_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Lighten Operation ******** */

MixLightenOperation::MixLightenOperation() : MixBaseOperation()
{
	/* pass */
}

void MixLightenOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_lighten_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Linear Light Operation ******** */

MixLinearLightOperation::MixLinearLightOperation() : MixBaseOperation()
{
	/* pass */
}

void MixLinearLightOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_linearlight_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Multiply Operation ******** */

MixMultiplyOperation::MixMultiplyOperation() : MixBaseOperation()
{
	/* pass */
}

void MixMultiplyOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_mul_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Ovelray Operation ******** */

MixOverlayOperation::MixOverlayOperation() : MixBaseOperation()
{
	/* pass */
}

void MixOverlayOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_overlay_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Saturation Operation ******** */

MixSaturationOperation::MixSaturationOperation() : MixBaseOperation()
{
	/* pass */
}

void MixSaturationOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_saturation_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Screen Operation ******** */

MixScreenOperation::MixScreenOperation() : MixBaseOperation()
{
	/* pass */
}

void MixScreenOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_screen_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Soft Light Operation ******** */

MixSoftLightOperation::MixSoftLightOperation() : MixBaseOperation()
{
	/* pass */
}

void MixSoftLightOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler) \
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_softlight_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Subtract Operation ******** */

MixSubtractOperation::MixSubtractOperation() : MixBaseOperation()
{
	/* pass */
}

void MixSubtractOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue,   x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1,   x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2,   x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_sub_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}

/* ******** Mix Value Operation ******** */

MixValueOperation::MixValueOperation() : MixBaseOperation()
{
	/* pass */
}

void MixValueOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	blend_color_luminosity_float_v4(output, inputColor1, inputColor2, value);

	clampIfNeeded(output);
}
