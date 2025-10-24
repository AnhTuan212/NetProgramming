# 1. Sửa lỗi Định danh người dùng (chỉ cần làm 1 lần)
git config --global user.email "natvn313@gmail.com"
git config --global user.name "AnhTuan212"

# 2. Xóa và Thêm lại remote
git remote remove origin
git remote add origin https://github.com/AnhTuan212/NetProgramming.git

# 3. Đảm bảo tất cả tệp đã được thêm và commit
git add .
git commit -m "Updated files for push"

# 4. Đổi tên branch thành 'main' (nếu cần)
git branch -M main

# 5. Thực hiện Push
git push -u origin main