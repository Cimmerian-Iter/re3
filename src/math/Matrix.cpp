#include "common.h"

CMatrix::CMatrix(void)
{
	m_attachment = nil;
	m_hasRwMatrix = false;
}

CMatrix::CMatrix(CMatrix const &m)
{
	m_attachment = nil;
	m_hasRwMatrix = false;
	*this = m;
}

CMatrix::CMatrix(RwMatrix *matrix, bool owner)
{
	m_attachment = nil;
	Attach(matrix, owner);
}

CMatrix::~CMatrix(void)
{
	if (m_hasRwMatrix && m_attachment)
		RwMatrixDestroy(m_attachment);
}

void
CMatrix::Attach(RwMatrix *matrix, bool owner)
{
#ifdef FIX_BUGS
	if (m_attachment && m_hasRwMatrix)
#else
	if (m_hasRwMatrix && m_attachment)
#endif
		RwMatrixDestroy(m_attachment);
	m_attachment = matrix;
	m_hasRwMatrix = owner;
	Update();
}

void
CMatrix::AttachRW(RwMatrix *matrix, bool owner)
{
	if (m_hasRwMatrix && m_attachment)
		RwMatrixDestroy(m_attachment);
	m_attachment = matrix;
	m_hasRwMatrix = owner;
	UpdateRW();
}

void
CMatrix::Detach(void)
{
	if (m_hasRwMatrix && m_attachment)
		RwMatrixDestroy(m_attachment);
	m_attachment = nil;
}

void
CMatrix::Update(void)
{
	m_matrix = *m_attachment;
}

void
CMatrix::UpdateRW(void)
{
	if (m_attachment) {
		*m_attachment = m_matrix;
		RwMatrixUpdate(m_attachment);
	}
}

void
CMatrix::operator=(CMatrix const &rhs)
{
	m_matrix = rhs.m_matrix;
	if (m_attachment)
		UpdateRW();
}

void
CMatrix::CopyOnlyMatrix(CMatrix *other)
{
	m_matrix = other->m_matrix;
}

CMatrix &
CMatrix::operator+=(CMatrix const &rhs)
{
	m_matrix.right.x += rhs.m_matrix.right.x;
	m_matrix.up.x += rhs.m_matrix.up.x;
	m_matrix.at.x += rhs.m_matrix.at.x;
	m_matrix.right.y += rhs.m_matrix.right.y;
	m_matrix.up.y += rhs.m_matrix.up.y;
	m_matrix.at.y += rhs.m_matrix.at.y;
	m_matrix.right.z += rhs.m_matrix.right.z;
	m_matrix.up.z += rhs.m_matrix.up.z;
	m_matrix.at.z += rhs.m_matrix.at.z;
	m_matrix.pos.x += rhs.m_matrix.pos.x;
	m_matrix.pos.y += rhs.m_matrix.pos.y;
	m_matrix.pos.z += rhs.m_matrix.pos.z;
	return *this;
}

void
CMatrix::SetUnity(void)
{
	m_matrix.right.x = 1.0f;
	m_matrix.right.y = 0.0f;
	m_matrix.right.z = 0.0f;
	m_matrix.up.x = 0.0f;
	m_matrix.up.y = 1.0f;
	m_matrix.up.z = 0.0f;
	m_matrix.at.x = 0.0f;
	m_matrix.at.y = 0.0f;
	m_matrix.at.z = 1.0f;
	m_matrix.pos.x = 0.0f;
	m_matrix.pos.y = 0.0f;
	m_matrix.pos.z = 0.0f;
}

void
CMatrix::ResetOrientation(void)
{
	m_matrix.right.x = 1.0f;
	m_matrix.right.y = 0.0f;
	m_matrix.right.z = 0.0f;
	m_matrix.up.x = 0.0f;
	m_matrix.up.y = 1.0f;
	m_matrix.up.z = 0.0f;
	m_matrix.at.x = 0.0f;
	m_matrix.at.y = 0.0f;
	m_matrix.at.z = 1.0f;
}

void
CMatrix::SetScale(float s)
{
	m_matrix.right.x = s;
	m_matrix.right.y = 0.0f;
	m_matrix.right.z = 0.0f;

	m_matrix.up.x = 0.0f;
	m_matrix.up.y = s;
	m_matrix.up.z = 0.0f;

	m_matrix.at.x = 0.0f;
	m_matrix.at.y = 0.0f;
	m_matrix.at.z = s;

	m_matrix.pos.x = 0.0f;
	m_matrix.pos.y = 0.0f;
	m_matrix.pos.z = 0.0f;
}

void
CMatrix::SetTranslate(float x, float y, float z)
{
	m_matrix.right.x = 1.0f;
	m_matrix.right.y = 0.0f;
	m_matrix.right.z = 0.0f;

	m_matrix.up.x = 0.0f;
	m_matrix.up.y = 1.0f;
	m_matrix.up.z = 0.0f;

	m_matrix.at.x = 0.0f;
	m_matrix.at.y = 0.0f;
	m_matrix.at.z = 1.0f;

	m_matrix.pos.x = x;
	m_matrix.pos.y = y;
	m_matrix.pos.z = z;
}

void
CMatrix::SetRotateXOnly(float angle)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(angle, cs);
#else
	float c = Cos(angle);
	float s = Sin(angle);
#endif
	m_matrix.right.x = 1.0f;
	m_matrix.right.y = 0.0f;
	m_matrix.right.z = 0.0f;

	m_matrix.up.x = 0.0f;
#ifdef PSP2
	m_matrix.up.y = cs[1];
	m_matrix.up.z = cs[0];

	m_matrix.at.x = 0.0f;
	m_matrix.at.y = -cs[0];
	m_matrix.at.z = cs[1];
#else
	m_matrix.up.y = c;
	m_matrix.up.z = s;

	m_matrix.at.x = 0.0f;
	m_matrix.at.y = -s;
	m_matrix.at.z = c;
#endif
}

void
CMatrix::SetRotateYOnly(float angle)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(angle, cs);

	m_matrix.right.x = cs[1];
	m_matrix.right.y = 0.0f;
	m_matrix.right.z = -cs[0];
#else
	float c = Cos(angle);
	float s = Sin(angle);

	m_matrix.right.x = c;
	m_matrix.right.y = 0.0f;
	m_matrix.right.z = -s;
#endif
	m_matrix.up.x = 0.0f;
	m_matrix.up.y = 1.0f;
	m_matrix.up.z = 0.0f;
#ifdef PSP2
	m_matrix.at.x = cs[0];
	m_matrix.at.y = 0.0f;
	m_matrix.at.z = cs[1];
#else
	m_matrix.at.x = s;
	m_matrix.at.y = 0.0f;
	m_matrix.at.z = c;
#endif
}

void
CMatrix::SetRotateZOnly(float angle)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(angle, cs);

	m_matrix.right.x = cs[1];
	m_matrix.right.y = cs[0];
	m_matrix.right.z = 0.0f;

	m_matrix.up.x = -cs[0];
	m_matrix.up.y = cs[1];
	m_matrix.up.z = 0.0f;
#else
	float c = Cos(angle);
	float s = Sin(angle);

	m_matrix.right.x = c;
	m_matrix.right.y = s;
	m_matrix.right.z = 0.0f;

	m_matrix.up.x = -s;
	m_matrix.up.y = c;
	m_matrix.up.z = 0.0f;
#endif
	m_matrix.at.x = 0.0f;
	m_matrix.at.y = 0.0f;
	m_matrix.at.z = 1.0f;
}

void
CMatrix::SetRotateX(float angle)
{
	SetRotateXOnly(angle);
	m_matrix.pos.x = 0.0f;
	m_matrix.pos.y = 0.0f;
	m_matrix.pos.z = 0.0f;
}


void
CMatrix::SetRotateY(float angle)
{
	SetRotateYOnly(angle);
	m_matrix.pos.x = 0.0f;
	m_matrix.pos.y = 0.0f;
	m_matrix.pos.z = 0.0f;
}

void
CMatrix::SetRotateZ(float angle)
{
	SetRotateZOnly(angle);
	m_matrix.pos.x = 0.0f;
	m_matrix.pos.y = 0.0f;
	m_matrix.pos.z = 0.0f;
}

void
CMatrix::SetRotate(float xAngle, float yAngle, float zAngle)
{
#ifdef PSP2
	float csX[2];
	float csY[2];
	float csZ[2];
	sincosf_c(xAngle, csX);
	sincosf_c(yAngle, csY);
	sincosf_c(zAngle, csZ);

	m_matrix.right.x = csZ[1] * csY[1] - (csZ[0] * csX[0]) * csY[0];
	m_matrix.right.y = (csZ[1] * csX[0]) * csY[0] + csZ[0] * csY[1];
	m_matrix.right.z = -csX[1] * csY[0];

	m_matrix.up.x = -csZ[0] * csX[1];
	m_matrix.up.y = csZ[1] * csX[1];
	m_matrix.up.z = csX[0];

	m_matrix.at.x = (csZ[0] * csX[0]) * csY[1] + csZ[1] * csY[0];
	m_matrix.at.y = csZ[0] * csY[0] - (csZ[1] * csX[0]) * csY[1];
	m_matrix.at.z = csX[1] * csY[1];
#else
	float cX = Cos(xAngle);
	float sX = Sin(xAngle);
	float cY = Cos(yAngle);
	float sY = Sin(yAngle);
	float cZ = Cos(zAngle);
	float sZ = Sin(zAngle);

	m_matrix.right.x = cZ * cY - (sZ * sX) * sY;
	m_matrix.right.y = (cZ * sX) * sY + sZ * cY;
	m_matrix.right.z = -cX * sY;

	m_matrix.up.x = -sZ * cX;
	m_matrix.up.y = cZ * cX;
	m_matrix.up.z = sX;

	m_matrix.at.x = (sZ * sX) * cY + cZ * sY;
	m_matrix.at.y = sZ * sY - (cZ * sX) * cY;
	m_matrix.at.z = cX * cY;
#endif
	m_matrix.pos.x = 0.0f;
	m_matrix.pos.y = 0.0f;
	m_matrix.pos.z = 0.0f;
}

void
CMatrix::RotateX(float x)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(x, cs);
#else
	float c = Cos(x);
	float s = Sin(x);
#endif
	float ry = m_matrix.right.y;
	float rz = m_matrix.right.z;
	float uy = m_matrix.up.y;
	float uz = m_matrix.up.z;
	float ay = m_matrix.at.y;
	float az = m_matrix.at.z;
	float py = m_matrix.pos.y;
	float pz = m_matrix.pos.z;
#ifdef PSP2
	m_matrix.right.y = cs[1] * ry - cs[0] * rz;
	m_matrix.right.z = cs[1] * rz + cs[0] * ry;
	m_matrix.up.y = cs[1] * uy - cs[0] * uz;
	m_matrix.up.z = cs[1] * uz + cs[0] * uy;
	m_matrix.at.y = cs[1] * ay - cs[0] * az;
	m_matrix.at.z = cs[1] * az + cs[0] * ay;
	m_matrix.pos.y = cs[1] * py - cs[0] * pz;
	m_matrix.pos.z = cs[1] * pz + cs[0] * py;
#else
	m_matrix.right.y = c * ry - s * rz;
	m_matrix.right.z = c * rz + s * ry;
	m_matrix.up.y = c * uy - s * uz;
	m_matrix.up.z = c * uz + s * uy;
	m_matrix.at.y = c * ay - s * az;
	m_matrix.at.z = c * az + s * ay;
	m_matrix.pos.y = c * py - s * pz;
	m_matrix.pos.z = c * pz + s * py;
#endif
}

void
CMatrix::RotateY(float y)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(y, cs);
#else
	float c = Cos(y);
	float s = Sin(y);
#endif
	float rx = m_matrix.right.x;
	float rz = m_matrix.right.z;
	float ux = m_matrix.up.x;
	float uz = m_matrix.up.z;
	float ax = m_matrix.at.x;
	float az = m_matrix.at.z;
	float px = m_matrix.pos.x;
	float pz = m_matrix.pos.z;
#ifdef PSP2
	m_matrix.right.x = cs[1] * rx + cs[0] * rz;
	m_matrix.right.z = cs[1] * rz - cs[0] * rx;
	m_matrix.up.x = cs[1] * ux + cs[0] * uz;
	m_matrix.up.z = cs[1] * uz - cs[0] * ux;
	m_matrix.at.x = cs[1] * ax + cs[0] * az;
	m_matrix.at.z = cs[1] * az - cs[0] * ax;
	m_matrix.pos.x = cs[1] * px + cs[0] * pz;
	m_matrix.pos.z = cs[1] * pz - cs[0] * px;
#else
	m_matrix.right.x = c * rx + s * rz;
	m_matrix.right.z = c * rz - s * rx;
	m_matrix.up.x = c * ux + s * uz;
	m_matrix.up.z = c * uz - s * ux;
	m_matrix.at.x = c * ax + s * az;
	m_matrix.at.z = c * az - s * ax;
	m_matrix.pos.x = c * px + s * pz;
	m_matrix.pos.z = c * pz - s * px;
#endif
}

void
CMatrix::RotateZ(float z)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(z, cs);
#else
	float c = Cos(z);
	float s = Sin(z);
#endif
	float ry = m_matrix.right.y;
	float rx = m_matrix.right.x;
	float uy = m_matrix.up.y;
	float ux = m_matrix.up.x;
	float ay = m_matrix.at.y;
	float ax = m_matrix.at.x;
	float py = m_matrix.pos.y;
	float px = m_matrix.pos.x;
#ifdef PSP2
	m_matrix.right.x = cs[1] * rx - cs[0] * ry;
	m_matrix.right.y = cs[1] * ry + cs[0] * rx;
	m_matrix.up.x = cs[1] * ux - cs[0] * uy;
	m_matrix.up.y = cs[1] * uy + cs[0] * ux;
	m_matrix.at.x = cs[1] * ax - cs[0] * ay;
	m_matrix.at.y = cs[1] * ay + cs[0] * ax;
	m_matrix.pos.x = cs[1] * px - cs[0] * py;
	m_matrix.pos.y = cs[1] * py + cs[0] * px;
#else
	m_matrix.right.x = c * rx - s * ry;
	m_matrix.right.y = c * ry + s * rx;
	m_matrix.up.x = c * ux - s * uy;
	m_matrix.up.y = c * uy + s * ux;
	m_matrix.at.x = c * ax - s * ay;
	m_matrix.at.y = c * ay + s * ax;
	m_matrix.pos.x = c * px - s * py;
	m_matrix.pos.y = c * py + s * px;
#endif
}

void
CMatrix::Rotate(float x, float y, float z)
{
#ifdef PSP2
	float csX[2];
	float csY[2];
	float csZ[2];
	sincosf_c(x, csX);
	sincosf_c(y, csY);
	sincosf_c(z, csZ);
#else
	float cX = Cos(x);
	float sX = Sin(x);
	float cY = Cos(y);
	float sY = Sin(y);
	float cZ = Cos(z);
	float sZ = Sin(z);
#endif
	float rx = m_matrix.right.x;
	float ry = m_matrix.right.y;
	float rz = m_matrix.right.z;
	float ux = m_matrix.up.x;
	float uy = m_matrix.up.y;
	float uz = m_matrix.up.z;
	float ax = m_matrix.at.x;
	float ay = m_matrix.at.y;
	float az = m_matrix.at.z;
	float px = m_matrix.pos.x;
	float py = m_matrix.pos.y;
	float pz = m_matrix.pos.z;
#ifdef PSP2
	float x1 = csZ[1] * csY[1] - (csZ[0] * csX[0]) * csY[0];
	float x2 = (csZ[1] * csX[0]) * csY[0] + csZ[0] * csY[1];
	float x3 = -csX[1] * csY[0];
	float y1 = -csZ[0] * csX[1];
	float y2 = csZ[1] * csX[1];
	float y3 = csX[0];
	float z1 = (csZ[0] * csX[0]) * csY[1] + csZ[1] * csY[0];
	float z2 = csZ[0] * csY[0] - (csZ[1] * csX[0]) * csY[1];
	float z3 = csX[1] * csY[1];
#else
	float x1 = cZ * cY - (sZ * sX) * sY;
	float x2 = (cZ * sX) * sY + sZ * cY;
	float x3 = -cX * sY;
	float y1 = -sZ * cX;
	float y2 = cZ * cX;
	float y3 = sX;
	float z1 = (sZ * sX) * cY + cZ * sY;
	float z2 = sZ * sY - (cZ * sX) * cY;
	float z3 = cX * cY;
#endif
	m_matrix.right.x = x1 * rx + y1 * ry + z1 * rz;
	m_matrix.right.y = x2 * rx + y2 * ry + z2 * rz;
	m_matrix.right.z = x3 * rx + y3 * ry + z3 * rz;
	m_matrix.up.x = x1 * ux + y1 * uy + z1 * uz;
	m_matrix.up.y = x2 * ux + y2 * uy + z2 * uz;
	m_matrix.up.z = x3 * ux + y3 * uy + z3 * uz;
	m_matrix.at.x = x1 * ax + y1 * ay + z1 * az;
	m_matrix.at.y = x2 * ax + y2 * ay + z2 * az;
	m_matrix.at.z = x3 * ax + y3 * ay + z3 * az;
	m_matrix.pos.x = x1 * px + y1 * py + z1 * pz;
	m_matrix.pos.y = x2 * px + y2 * py + z2 * pz;
	m_matrix.pos.z = x3 * px + y3 * py + z3 * pz;
}

CMatrix &
CMatrix::operator*=(CMatrix const &rhs)
{
	// TODO: VU0 code
	*this = *this * rhs;
	return *this;
}

void
CMatrix::Reorthogonalise(void)
{
	CVector &r = GetRight();
	CVector &f = GetForward();
	CVector &u = GetUp();
	u = CrossProduct(r, f);
	u.Normalise();
	r = CrossProduct(f, u);
	r.Normalise();
	f = CrossProduct(u, r);
}

CMatrix
operator*(const CMatrix &m1, const CMatrix &m2)
{
	// TODO: VU0 code
	CMatrix out;
	RwMatrix *dst = &out.m_matrix;
	const RwMatrix *src1 = &m1.m_matrix;
	const RwMatrix *src2 = &m2.m_matrix;
	dst->right.x = src1->right.x * src2->right.x + src1->up.x * src2->right.y + src1->at.x * src2->right.z;
	dst->right.y = src1->right.y * src2->right.x + src1->up.y * src2->right.y + src1->at.y * src2->right.z;
	dst->right.z = src1->right.z * src2->right.x + src1->up.z * src2->right.y + src1->at.z * src2->right.z;
	dst->up.x = src1->right.x * src2->up.x + src1->up.x * src2->up.y + src1->at.x * src2->up.z;
	dst->up.y = src1->right.y * src2->up.x + src1->up.y * src2->up.y + src1->at.y * src2->up.z;
	dst->up.z = src1->right.z * src2->up.x + src1->up.z * src2->up.y + src1->at.z * src2->up.z;
	dst->at.x = src1->right.x * src2->at.x + src1->up.x * src2->at.y + src1->at.x * src2->at.z;
	dst->at.y = src1->right.y * src2->at.x + src1->up.y * src2->at.y + src1->at.y * src2->at.z;
	dst->at.z = src1->right.z * src2->at.x + src1->up.z * src2->at.y + src1->at.z * src2->at.z;
	dst->pos.x = src1->right.x * src2->pos.x + src1->up.x * src2->pos.y + src1->at.x * src2->pos.z + src1->pos.x;
	dst->pos.y = src1->right.y * src2->pos.x + src1->up.y * src2->pos.y + src1->at.y * src2->pos.z + src1->pos.y;
	dst->pos.z = src1->right.z * src2->pos.x + src1->up.z * src2->pos.y + src1->at.z * src2->pos.z + src1->pos.z;
	return out;
}

CMatrix &
Invert(const CMatrix &src, CMatrix &dst)
{
	// TODO: VU0 code
	float (*scr_fm)[4] = (float (*)[4])&src.m_matrix;
	float (*dst_fm)[4] = (float (*)[4])&dst.m_matrix;

	dst_fm[3][0] = dst_fm[3][1] = dst_fm[3][2] = 0.0f;

	dst_fm[0][0] = scr_fm[0][0];
	dst_fm[0][1] = scr_fm[1][0];
	dst_fm[0][2] = scr_fm[2][0];

	dst_fm[1][0] = scr_fm[0][1];
	dst_fm[1][1] = scr_fm[1][1];
	dst_fm[1][2] = scr_fm[2][1];

	dst_fm[2][0] = scr_fm[0][2];
	dst_fm[2][1] = scr_fm[1][2];
	dst_fm[2][2] = scr_fm[2][2];


	dst_fm[3][0] += dst_fm[0][0] * scr_fm[3][0];
	dst_fm[3][1] += dst_fm[0][1] * scr_fm[3][0];
	dst_fm[3][2] += dst_fm[0][2] * scr_fm[3][0];

	dst_fm[3][0] += dst_fm[1][0] * scr_fm[3][1];
	dst_fm[3][1] += dst_fm[1][1] * scr_fm[3][1];
	dst_fm[3][2] += dst_fm[1][2] * scr_fm[3][1];

	dst_fm[3][0] += dst_fm[2][0] * scr_fm[3][2];
	dst_fm[3][1] += dst_fm[2][1] * scr_fm[3][2];
	dst_fm[3][2] += dst_fm[2][2] * scr_fm[3][2];

	dst_fm[3][0] = -dst_fm[3][0];
	dst_fm[3][1] = -dst_fm[3][1];
	dst_fm[3][2] = -dst_fm[3][2];

	return dst;
}

CMatrix
Invert(const CMatrix &matrix)
{
	CMatrix inv;
	return Invert(matrix, inv);
}

void
CCompressedMatrixNotAligned::CompressFromFullMatrix(CMatrix &other)
{
	m_rightX = 127.0f * other.GetRight().x;
	m_rightY = 127.0f * other.GetRight().y;
	m_rightZ = 127.0f * other.GetRight().z;
	m_upX = 127.0f * other.GetForward().x;
	m_upY = 127.0f * other.GetForward().y;
	m_upZ = 127.0f * other.GetForward().z;
	m_vecPos = other.GetPosition();
}

void
CCompressedMatrixNotAligned::DecompressIntoFullMatrix(CMatrix &other)
{
	other.GetRight().x = m_rightX / 127.0f;
	other.GetRight().y = m_rightY / 127.0f;
	other.GetRight().z = m_rightZ / 127.0f;
	other.GetForward().x = m_upX / 127.0f;
	other.GetForward().y = m_upY / 127.0f;
	other.GetForward().z = m_upZ / 127.0f;
	other.GetUp() = CrossProduct(other.GetRight(), other.GetForward());
	other.GetPosition() = m_vecPos;
	other.Reorthogonalise();
}