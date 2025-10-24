

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
